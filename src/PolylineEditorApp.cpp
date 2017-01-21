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
	void mouseUp( MouseEvent event ) override;
	void keyUp( KeyEvent event ) override;
	void resize() override;
	void update() override;
	void draw() override;
	void drawAddCursor( const vec2 &at );
	void drawRemoveCursor( const vec2 &at );
	void drawMoveCursor( const vec2 &at );

	// Using a list so the iterators are valid after insertions and removals.
	typedef typename list<PolyLine2f>::iterator FaceIterator;
	typedef typename vector<vec2>::iterator		VertexIterator;
	typedef typename std::pair<vec2, vec2>		Edge;

	bool hasHoverFace() const { return mFaceHover != mPolyLines.end(); }
	void setHoverFace( const FaceIterator &face ) { mFaceHover = face; }
	void clearHoverFace() { mFaceHover = mPolyLines.end(); }

	bool hasSelectedFace() const { return mFaceSelected != mPolyLines.end(); };
	void selectFace( const FaceIterator &face ) {
		mFaceSelected = face;
		mVertHover = mFaceSelected->end();
	}
	void deselectFace() { mFaceSelected = mPolyLines.end(); };
	void selectFirstFace() { selectFace( mPolyLines.begin() ); }
	void selectNextFace() {
		++mFaceSelected;
		if( hasSelectedFace() )
			mVertHover = mFaceSelected->end();
		else
			selectFirstFace();
	}

	// Applies the camera projection to determine where \a mouse lies on the
	// plane. If this returns true then \a onPlane will be the result.
	bool positionOnPlane( const vec2 &mouse, vec2 &onPlane ) const;
	// Find the face that is contained by the cursor.
	FaceIterator faceContainedBy( const vec2 &cursor, std::list<PolyLine2f> &polylines );
	// Looks for a point with in \a distance of \a cursor. Returns iterator in \a points.
	VertexIterator findVertexNear( const vec2 &cursor, std::vector<vec2> &points, float distance ) const;
	// Looks for where you would insert a point to split an edge in \points. The
	// cursor must be within distance of the edge. \a target is the point on the edge.
	// Returns iterator in \a points where insertion should occur.
	VertexIterator findSplitPoint( const vec2 &cursor, std::vector<vec2> &points, float distance, vec2 &target ) const;

	bool isAppending() const { return mPolyLines.size() && ! mPolyLines.back().isClosed(); }

	enum OverWhat {
		NADA,
		FACE,
		EDGE,
		VERT,
	};

  private:
	list<PolyLine2f>	mPolyLines;
	u_int8_t		mHoverRadius = 20;
	OverWhat		mHoverOver;		// What are they hovering over?
	FaceIterator	mFaceHover;		// The face under the cursor
	FaceIterator	mFaceSelected;	// The face they've selected
	vec2			mInsertPoint;
	VertexIterator	mInsertAfter;
	VertexIterator	mVertHover;
    CameraPersp mEditCamera;

	bool		mIsDragging;
	vec2		mMousePosition;
	vec2		mCursorPosition; // Is on the plane
	vec2		mLastMouseDrag; //
};

void PolylineEditorApp::setup()
{
    mEditCamera.setPerspective( 60.0f, getWindowAspectRatio(), 10.0f, 4000.0f );
    mEditCamera.lookAt( vec3( 0, 0, 1000 ), vec3( 0 ), vec3( 0, 1, 0 ) );

	mHoverOver = NADA;
	mIsDragging = false;

	mPolyLines.push_back( PolyLine2f() );
	clearHoverFace();
	deselectFace();
}

void PolylineEditorApp::resize()
{
    mEditCamera.setAspectRatio( getWindowAspectRatio() );
}

void PolylineEditorApp::mouseMove( MouseEvent event )
{
	mHoverOver = NADA;
	mIsDragging = false;
	mMousePosition = event.getPos();
	positionOnPlane( mMousePosition, mCursorPosition );

	if( isAppending() ) {
		return;
	}

	if( hasSelectedFace() ) {
		mVertHover = findVertexNear( mCursorPosition, mFaceSelected->getPoints(), mHoverRadius );
		if( mVertHover != mFaceSelected->end() ) {
			mHoverOver = VERT;
		}
		else {
			mInsertAfter = findSplitPoint( mCursorPosition, mFaceSelected->getPoints(), mHoverRadius, mInsertPoint );
			if( mInsertAfter != mFaceSelected->end() ) {
				mHoverOver = EDGE;
			}
		}
	}

	setHoverFace( faceContainedBy( mCursorPosition, mPolyLines ) );
	if( hasHoverFace() && mHoverOver == NADA ) {
		mHoverOver = FACE;
	}
}

void PolylineEditorApp::mouseDown( MouseEvent event )
{
	// Process the select face on down so we can click and drag in one step
	if( mHoverOver == FACE ) {
		selectFace( mFaceHover );
	}
}

void PolylineEditorApp::mouseDrag( MouseEvent event )
{
	if( ! mIsDragging ) {
		mLastMouseDrag = mCursorPosition;
	}
	mIsDragging = true;

	positionOnPlane( event.getPos(), mCursorPosition );

	if( mHoverOver == VERT ) {
		*mVertHover = mCursorPosition;
	} else if( mHoverOver == FACE ) {
		vec2 delta( mCursorPosition - mLastMouseDrag );
		if( delta != vec2( 0 ) )
			mFaceSelected->offset( delta );
	}

	mLastMouseDrag = mCursorPosition;
}

void PolylineEditorApp::mouseUp( MouseEvent event )
{
	if( mIsDragging ) {
		mIsDragging = false;
	} else if( this->isAppending() ) {
		mPolyLines.back().push_back( mCursorPosition );
	} else if( mHoverOver == EDGE ) {
		mFaceSelected->getPoints().insert( mInsertAfter, mInsertPoint );
	} else if( mHoverOver != FACE ) {
		deselectFace();
	}
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
			deselectFace();
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

			gl::color( Color::white() );
			for( auto const &vert : p->getPoints() ) {
				gl::drawSolidCircle( vert, mHoverRadius / 2, circleSegments );
			}

			if( mHoverOver == VERT ) {
				drawMoveCursor( *mVertHover );
			} else if( mHoverOver == EDGE ) {
				drawAddCursor( mInsertPoint );
			}
		} else if( mHoverOver == FACE && mFaceHover == p ) {
			gl::ScopedColor color( hover );
			gl::draw( *p );
		}
	}

	if( isAppending ) {
		// Draw the incomplete shape
		PolyLine2f copy( mPolyLines.back() );
		// TODO: would be cool if these last two lines could be dashed.
		copy.push_back( mCursorPosition );
		copy.setClosed();
		gl::draw( copy );

		// Draw the + cursor
		drawAddCursor( mCursorPosition );
	}
}

void PolylineEditorApp::drawAddCursor( const vec2 &at )
{
	gl::ScopedModelMatrix scopedMatrix;
	gl::translate( at );
	gl::drawSolidRect( Rectf( vec2( -5, 20 ), vec2( 5, -20 ) ) );
	gl::drawSolidRect( Rectf( vec2( -20, 5 ), vec2( 20, -5 ) ) );
}

void PolylineEditorApp::drawRemoveCursor( const vec2 &at )
{
	gl::ScopedModelMatrix scopedMatrix;
	gl::translate( at );
	gl::rotate( M_PI_4 );
	gl::drawSolidRect( Rectf( vec2( -5, 20 ), vec2( 5, -20 ) ) );
	gl::drawSolidRect( Rectf( vec2( -20, 5 ), vec2( 20, -5 ) ) );
}

void PolylineEditorApp::drawMoveCursor( const vec2 &at )
{
	gl::ScopedModelMatrix scopedMatrix;
	gl::translate( at );

	// TODO: should avoid this second scaling and just size the
	// triangles to match everything else.
	gl::scale( vec2( 5 ) );

	gl::drawSolidTriangle( vec2( 0,  5 ), vec2( -2,  3 ), vec2(  2,  3 ) );
	gl::drawSolidTriangle( vec2( 0, -5 ), vec2(  2, -3 ), vec2( -2, -3 ) );
	gl::drawSolidTriangle( vec2(  5, 0 ), vec2(  3, -2 ), vec2(  3,  2 ) );
	gl::drawSolidTriangle( vec2( -5, 0 ), vec2( -3,  2 ), vec2( -3, -2 ) );
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

PolylineEditorApp::FaceIterator PolylineEditorApp::faceContainedBy( const vec2 &needle, std::list<PolyLine2f> &haystack )
{
	return std::find_if( haystack.begin(), haystack.end(),
		[&]( const PolyLine2f &p ) { return p.contains( needle ); } );
}

PolylineEditorApp::VertexIterator PolylineEditorApp::findVertexNear( const vec2 &needle, std::vector<vec2> &haystack, float distance ) const
{
	float distance2 = distance * distance;
	return std::find_if( haystack.begin(), haystack.end(), [&]( const vec2 &p ) {
		return glm::distance2( p, needle ) < distance2;
	});
}

PolylineEditorApp::VertexIterator PolylineEditorApp::findSplitPoint( const vec2 &cursor, std::vector<vec2> &points, float distance, vec2 &target ) const
{
	float distance2 = distance * distance;

	if( points.size() < 2 ) return points.end();

	auto distanceCheck = [&]( const vec2 &a, const vec2 & b ) {
		vec2 closest = getClosestPointLinear( a, b, cursor );
		if( glm::distance2( closest, cursor ) < distance2 ) {
			target = closest;
			return true;
		}
		return false;
	};

	for( auto prev = points.begin(), curr = prev + 1; curr != points.end(); ++curr ) {
		if( distanceCheck( *prev, *curr ) )
			return curr;
		prev = curr;
	}
	if( mFaceSelected->isClosed() && points.front() != points.back() ) {
		if( distanceCheck( points.back(), points.front() ) )
			return points.begin();
	}

	return points.end();
}

void prepareSettings( App::Settings *settings )
{
    settings->setHighDensityDisplayEnabled();
}
CINDER_APP( PolylineEditorApp, RendererGl, prepareSettings )
