#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Camera.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class PolylineEditorApp : public App {
  public:
	void setup() override;
	void mouseMove( MouseEvent event ) override;
	void mouseDown( MouseEvent event ) override;
	void mouseDrag( MouseEvent event ) override;
	void keyUp( KeyEvent event ) override;
	void resize() override;
	void update() override;
	void draw() override;

	// Using a list so the iterators are valid after insertions and removals.
	typedef typename list<PolyLine2f>::iterator Iterator;
	typedef typename std::pair<vec2, vec2>		Edge;

	bool hasSelectedFace() const { return mFaceSelected != mPolyLines.end(); };
	void selectFace( Iterator &face ) { mFaceSelected = face; }
	void deselectFace() { mFaceSelected = mPolyLines.end(); };
	void selectFirstFace() { mFaceSelected = mPolyLines.begin(); }
	void selectNextFace() {
		++mFaceSelected;
		if( ! hasSelectedFace() )
			selectFirstFace();
	}

	// Applies the camera projection to determine where \a mouse lies on the
	// plane. If this returns true then \a onPlane will be the result.
	bool positionOnPlane( const vec2 &mouse, vec2 &onPlane ) const;
	Iterator faceContainedBy( const vec2 &onPlane );
	// Is the cursor over a vertex in the selected face (plus a \a margin)? If
	// this returns true \a result will be the point.
	bool isOverVertex( const vec2 &cursor, vec2 &result, float margin = 10 ) const;
	// Is the cursor over an edge in the selected face (plus a \a margin)? If this
	// returns true \a result will be the edge and \at will be the closest point
	// to the cursor.
	bool isOverEdge( const vec2 &cursor, Edge &result, vec2 &at, float margin = 10 ) const;

	bool isAppending() const { return mPolyLines.size() && ! mPolyLines.back().isClosed(); }

	list<PolyLine2f>	mPolyLines;
	Iterator	mFaceHover;		// The face the cursor is over
	Iterator	mFaceSelected;	// The face they've selected
	vec2		mVertexHover;	// Vertex in the selected face, only valid
    CameraPersp mEditCamera;
	vec2		mMousePosition;
	vec2		mCursorPosition; // Is on the plane
	ivec2		mLastMouseDrag;
};

void PolylineEditorApp::setup()
{
    mEditCamera.setPerspective( 60.0f, getWindowAspectRatio(), 10.0f, 4000.0f );
    mEditCamera.lookAt( vec3( 0, 0, 1000 ), vec3( 0 ), vec3( 0, 1, 0 ) );

	mPolyLines.push_back( PolyLine2f() );
}

void PolylineEditorApp::resize()
{
    mEditCamera.setAspectRatio( getWindowAspectRatio() );
}

void PolylineEditorApp::mouseMove( MouseEvent event )
{
	mMousePosition = event.getPos();
	positionOnPlane( mMousePosition, mCursorPosition );
	if( ! isAppending() ) {
		mFaceHover = this->faceContainedBy( mCursorPosition );
	}
}

void PolylineEditorApp::mouseDown( MouseEvent event )
{
	if( this->isAppending() ) {
		mPolyLines.back().push_back( mCursorPosition );
	} else {
		selectFace( mFaceHover );
	}

	// Keel a recent value so when dragging starts the delta will be sane.
	mLastMouseDrag = event.getPos();
}

void PolylineEditorApp::mouseDrag( MouseEvent event ) {
	if( isAppending() ) return;

	// Something isn't quite right here. It's dragging at half speed.
	ivec2 delta( event.getPos() - mLastMouseDrag );
	if( delta != ivec2( 0 ) && hasSelectedFace() ) {
		mFaceSelected->offset( delta * ivec2( 1, -1 ) );
	}
	mLastMouseDrag = event.getPos();
}


void PolylineEditorApp::keyUp( KeyEvent event )
{
	switch( event.getCode() ) {
		case KeyEvent::KEY_ESCAPE:
			if( isAppending() )
				mPolyLines.pop_back();
			else
				deselectFace();
			break;
		case KeyEvent::KEY_RETURN:
			if( mPolyLines.size() )
				mPolyLines.back().setClosed();
			break;
		case KeyEvent::KEY_TAB:
			// Ignore Alt+Tab or Command+Tab
			if( ! ( event.isAltDown() || event.isMetaDown() ) )
				selectNextFace();
			break;
		case KeyEvent::KEY_BACKSPACE:
			if( hasSelectedFace() ) {
				mPolyLines.erase( mFaceSelected );
				deselectFace();
			}
			break;
		case KeyEvent::KEY_n:
			if( isAppending() )
				mPolyLines.back().setClosed();
			mPolyLines.push_back( PolyLine2f() );
			break;
	}
}

void PolylineEditorApp::update()
{
}

void PolylineEditorApp::draw()
{
	ColorA hover( 1, 0, 1, 1 );
	ColorA selected( 0, 1, 1, 1 );
	u_int8_t circleRadius( 20 );
	u_int8_t circleSegments( 8 );

	gl::clear( Color( 0, 0, 0 ) );

	gl::ScopedMatrices matrixScope;
	gl::setMatrices( mEditCamera );

	if( mPolyLines.size() == 0 ) return;

	bool isAppending = this->isAppending();
	auto end = mPolyLines.end();
	if( isAppending )
		--end;

	// Draw completed shapes
	for( auto p = mPolyLines.begin(); p != end; ++p ) {
		gl::ScopedColor color( ColorA( 1, 1, 1, 0.5 ) );
		gl::drawSolid( *p );
		if( mFaceSelected == p ) {
			gl::ScopedColor color( selected );
			gl::draw( *p );

			vec2 vert;
			Edge edge;
			if( isOverVertex( mCursorPosition, vert, circleRadius ) ) {
				gl::ScopedColor color( Color( 1, 1, 0 ) );
				gl::drawSolidCircle( vert, circleRadius, circleSegments );
			} else if( isOverEdge( mCursorPosition, edge, vert, circleRadius ) ) {
				gl::ScopedColor color( Color( 1, 1, 0 ) );
				gl::drawLine( edge.first, edge.second );
				gl::drawSolidCircle( vert, circleRadius, circleSegments );
			}
		} else if( mFaceHover == p ) {
			gl::ScopedColor color( hover );
			gl::draw( *p );
		}
	}

	if( isAppending ) {
		// Draw the incomplete shape
		PolyLine2f copy( mPolyLines.back() );
		copy.push_back( mCursorPosition );
		copy.setClosed();
		gl::draw( copy );

		// Draw the + cursor
		gl::ScopedModelMatrix scopedMatrix;
		gl::translate( mCursorPosition );
		gl::drawSolidRect( Rectf( vec2( -5, 20 ), vec2( 5, -20 ) ) );
		gl::drawSolidRect( Rectf( vec2( -20, 5 ), vec2( 20, -5 ) ) );
	}
}

bool PolylineEditorApp::positionOnPlane( const vec2 &mouse, vec2 &onPlane ) const
{
    float u = mouse.x / (float) getWindowWidth();
    float v = ( getWindowHeight() - mouse.y ) / (float) getWindowHeight();
    Ray ray = mEditCamera.generateRay( u, v, mEditCamera.getAspectRatio() );
    float distance = 0.0f;
    if( ! ray.calcPlaneIntersection( glm::zero<ci::vec3>(), vec3( 0, 0, 1 ), &distance ) ) {
        return false;
    }
    vec3 intersection = ray.calcPosition( distance );
    onPlane = vec2( intersection.x, intersection.y );
    return true;
}


PolylineEditorApp::Iterator PolylineEditorApp::faceContainedBy( const vec2 &onPlane )
{
	return std::find_if( mPolyLines.begin(), mPolyLines.end(),
		[&onPlane]( const PolyLine2f &p ) { return p.contains( onPlane ); } );
}

bool PolylineEditorApp::isOverVertex( const vec2 &cursor, vec2 &result, float margin ) const
{
	std::vector<vec2> &points = mFaceSelected->getPoints();
	float margin2 = margin * margin;
	for( auto const &p : points ) {
		if( glm::distance2( p, cursor ) < margin2 ) {
			result = p;
			return true;
		}
	}
	return false;
}

bool PolylineEditorApp::isOverEdge( const vec2 &cursor, Edge &result, vec2 &at, float margin ) const
{
	std::vector<vec2> &points = mFaceSelected->getPoints();
	float margin2 = margin * margin;

	if( points.size() < 2 ) return false;

	auto distanceCheck = [&]( const vec2 &a, const vec2 & b ) {
		vec2 closest = getClosestPointLinear( a, b, cursor );
		if( glm::distance2( closest, cursor ) < margin2 ) {
			result.first = a;
			result.second = b;
			at = closest;
			return true;
		}
		return false;
	};

	for( auto prev = points.begin(), curr = prev + 1; curr != points.end(); ++curr ) {
		if( distanceCheck( *prev, *curr ) )
			return true;
		prev = curr;
	}
	if( mFaceSelected->isClosed() && points.front() != points.back() ) {
		if( distanceCheck( points.front(), points.back() ) )
			return true;
	}

	return false;
}

void prepareSettings( App::Settings *settings )
{
    settings->setHighDensityDisplayEnabled();
}
CINDER_APP( PolylineEditorApp, RendererGl, prepareSettings )
