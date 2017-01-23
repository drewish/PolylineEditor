/*
(select vert ahead of edge since they're smaller and harder to select)

verbs to support
- new (new face, append vertex)
- move (move entire face, move vertex, move both vertexes in edge?)
- remove (remove entire face, remove vertex)
- split (edge)

rework selection mode
- active face (changing clears vert and edge)
- active vert (allows: move, delete)
- active edge (allows: move, split, delete)

- hover face (select from all faces)
- hover vert (limited to active face)
- hover edge (limited to active face)
*/
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Camera.h"

using namespace ci;
using namespace ci::app;
using namespace std;

struct Focus {
	int face = -1;
	int vert = -1; //
	int edge = -1; // target side of source -> target

	int setFace( int f ) {
		clearVertEdge();
		return face = f;
	}
	int setVert( int v ) {
		edge = -1;
		return vert = v;
	}
	int setEdge( int e ) {
		vert = -1;
		return edge = e;
	}
	void clear() { face = vert = edge = -1; }
	void clearVertEdge() { vert = edge = -1; }
	bool hasFace() const { return face != -1; }
	bool hasVert() const { return hasFace() && vert != -1; }
	bool hasEdge() const { return hasFace() && edge != -1; }
};

int findFaceUnder( const vec2 &cursor, std::vector<PolyLine2f> &polylines )
{
	auto iterator = std::find_if( polylines.begin(), polylines.end(),
		[&]( const PolyLine2f &p ) { return p.contains( cursor ); } );
	if ( iterator == polylines.end() )
		return -1;
	return (int)std::distance( polylines.begin(), iterator );
}

int findVertexUnder( const vec2 &cursor, const PolyLine2 &polyline, float distance )
{
	float distance2 = distance * distance;
	const vector<vec2> &points = polyline.getPoints();

	auto iterator = std::find_if( points.begin(), points.end(), [&]( const vec2 &p ) {
		return glm::distance2( p, cursor ) < distance2;
	});
	if ( iterator == polyline.end() )
		return -1;
	return (int)std::distance( points.begin(), iterator );
}

int findEdgeUnder( const vec2 &cursor, const PolyLine2 &polyline, float distance, vec2 &closestPointOnEdge )
{
	float distance2 = distance * distance;
	const vector<vec2> &points = polyline.getPoints();

	if( points.size() < 2 ) return -1;

	auto distanceCheck = [&]( const vec2 &a, const vec2 & b ) {
		vec2 closest = getClosestPointLinear( a, b, cursor );
		if( glm::distance2( closest, cursor ) < distance2 ) {
			closestPointOnEdge = closest;
			return true;
		}
		return false;
	};

	for( auto prev = points.begin(), curr = prev + 1; curr != points.end(); ++curr ) {
		if( distanceCheck( *prev, *curr ) )
			return (int)std::distance( points.begin(), curr);
		prev = curr;
	}
	if( polyline.isClosed() && points.front() != points.back() ) {
		if( distanceCheck( points.back(), points.front() ) )
			return 0;
	}

	return -1;
}

void snapToGrid( vec2 &cursor, const vec2 &grid, bool doSnap = true )
{
	if( doSnap )
		cursor = grid * glm::round( cursor / grid );
}


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

	typedef std::pair<vec2&, vec2&> Edge;
	void setHoverFace( int face ) {
		mHover.setFace( face < mPolyLines.size() ? face : -1 );
	}
	PolyLine2& getHoverFace() { return mPolyLines[mHover.face]; }
	vec2& getHoverVertex() { return mPolyLines[mHover.face].getPoints().at(mHover.vert); }

	void setActiveFace( int face ) {
		mActive.setFace( face < mPolyLines.size() ? face : -1 );
	}
	void selectNextFace() {
		setActiveFace( mActive.face + 1 < mPolyLines.size() ? mActive.face + 1 : 0 );
	}
	PolyLine2& getActiveFace() { return mPolyLines.at(mActive.face); }
	vec2& getActiveVertex() { return getActiveFace().getPoints().at(mActive.vert); }
	Edge getActiveEdge() {
		vector<vec2> &points = getActiveFace().getPoints();
		int target = mActive.edge;
		int source = ( target == 0 ? (int)points.size() : target ) - 1;
		return Edge( points[source], points[target] );
	}

	// Applies the camera projection to determine where \a mouse lies on the
	// plane. If this returns true then \a onPlane will be the result.
	bool positionOnPlane( const vec2 &mouse, vec2 &onPlane ) const;

	bool isAppending() const { return mPolyLines.size() && ! mPolyLines.back().isClosed(); }

  private:
	vector<PolyLine2f>	mPolyLines;
	u_int8_t		mHoverRadius = 20;
	vec2			mInsertPoint;
	Focus			mActive;
	Focus			mHover;
    CameraPersp mEditCamera;

	bool		mIsSnapping = true;
	vec2		mGridSize = vec2( 25 );
	bool		mIsDragging;
	// Positions are corrdinates on the plane
	vec2		mCursorPosition;
	vec2		mLastCursorPosition;
};

void PolylineEditorApp::setup()
{
    mEditCamera.setPerspective( 60.0f, getWindowAspectRatio(), 10.0f, 4000.0f );
    mEditCamera.lookAt( vec3( 0, 0, 1000 ), vec3( 0 ), vec3( 0, 1, 0 ) );

	mIsDragging = false;

	mPolyLines.push_back( PolyLine2f() );
	mActive.clear();
	mHover.clear();
}

void PolylineEditorApp::resize()
{
    mEditCamera.setAspectRatio( getWindowAspectRatio() );
}

void PolylineEditorApp::mouseMove( MouseEvent event )
{
	mIsDragging = false;
	mLastCursorPosition = mCursorPosition;
	positionOnPlane( event.getPos(), mCursorPosition );
	snapToGrid( mCursorPosition, mGridSize, mIsSnapping );

	if( isAppending() ) {
		return;
	}

	setHoverFace( findFaceUnder( mCursorPosition, mPolyLines ) );
	if( mActive.hasFace() ) {
		int vert = findVertexUnder( mCursorPosition, getActiveFace(), mHoverRadius );
		if( vert != -1 ) {
			mHover.setFace( mActive.face );
			mHover.setVert( vert );
		} else {
			int edge = findEdgeUnder( mCursorPosition, getActiveFace(), mHoverRadius, mInsertPoint );
			if( edge != -1 ) {
				mHover.setFace( mActive.face );
				mHover.setEdge( edge );
				snapToGrid( mInsertPoint, mGridSize, mIsSnapping );
			}
		}
	}
}

void PolylineEditorApp::mouseDown( MouseEvent event )
{
	// Process the select face on down so we can click and drag in one step
	if( mHover.hasFace() ) {
		if( mActive.face != mHover.face ) {
			setActiveFace( mHover.face );
		} else if( mHover.hasVert() && mActive.vert != mHover.vert ) {
			mActive.setVert( mHover.vert );
		} else if( mHover.hasEdge() && mActive.edge != mHover.edge ) {
			mActive.setEdge( mHover.edge );
		}
	}
}

void PolylineEditorApp::mouseDrag( MouseEvent event )
{
	mIsDragging = true;
	mLastCursorPosition = mCursorPosition;
	positionOnPlane( event.getPos(), mCursorPosition );
	snapToGrid( mCursorPosition, mGridSize, mIsSnapping );

	vec2 delta( mCursorPosition - mLastCursorPosition );
	if( delta == vec2( 0 ) )
		return;

	if( mActive.hasVert() ) {
		// Move vertex.
		snapToGrid( getActiveVertex() += delta, mGridSize, mIsSnapping );
	} else if( mActive.hasEdge() ) {
		// Move both verticies in the edge.
		Edge edge = getActiveEdge();
		snapToGrid( edge.first += delta, mGridSize, mIsSnapping );
		snapToGrid( edge.second += delta, mGridSize, mIsSnapping );
		// Need to move this too so it doesn't reappear in the old spot.
		mInsertPoint = mCursorPosition;
	} else if( mActive.hasFace() ) {
		// Move face
		getActiveFace().offset( delta );
	}
}

void PolylineEditorApp::mouseUp( MouseEvent event )
{
	if( mIsDragging ) {
		mIsDragging = false;
		mActive.clearVertEdge();
	} else if( this->isAppending() ) {
		mPolyLines.back().push_back( mCursorPosition );
	} else if( mHover.hasEdge() ) {
		vector<vec2> &points = getActiveFace().getPoints();
		points.insert( points.begin() + mActive.edge, mInsertPoint );
		// Set the new vertex up to be moved immediately
		mActive.setVert( mActive.edge );
		mHover.setVert( mActive.edge );
	} else if( ! mHover.hasFace() ) {
		mActive.clear();
	}
}

void PolylineEditorApp::keyUp( KeyEvent event )
{
	switch( event.getCode() ) {
		case KeyEvent::KEY_ESCAPE:
			if( isAppending() )
				mPolyLines.pop_back();
			else
				mActive.clear();
			break;
		case KeyEvent::KEY_RETURN:
			if( isAppending() ) {
				mPolyLines.back().setClosed();
				setActiveFace( (int)mPolyLines.size() - 1 );
			}
			break;
		case KeyEvent::KEY_TAB:
			// Ignore Alt+Tab or Command+Tab
			if( ! ( event.isAltDown() || event.isMetaDown() ) )
				selectNextFace();
			break;
		case KeyEvent::KEY_BACKSPACE:
			if( mActive.hasVert() ) {
				vector<vec2> &points = getActiveFace().getPoints();
				points.erase( points.begin() + mActive.vert );
				mActive.clearVertEdge();
				mHover.clear();
			} else if( mActive.hasFace() ) {
				mPolyLines.erase( mPolyLines.begin() + mActive.face );
				setActiveFace( mActive.face - 1 );
				setHoverFace( mActive.face );
			}
			break;
		case KeyEvent::KEY_n:
			mActive.clear();
			if( isAppending() )
				mPolyLines.back().setClosed();
			mPolyLines.push_back( PolyLine2f() );
			break;
		case KeyEvent::KEY_g:
			mIsSnapping = ! mIsSnapping;
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
	size_t faceEnd = mPolyLines.size();
	if( isAppending )
		--faceEnd;

	// Draw completed shapes
	for( size_t faceIndex = 0; faceIndex < faceEnd; ++faceIndex ) {
		gl::ScopedColor color( ColorA( 1, 1, 1, 0.5 ) );
		gl::drawSolid( mPolyLines[faceIndex] );
		if( faceIndex == mActive.face ) {
			gl::ScopedColor color( selected );
			gl::draw( mPolyLines[faceIndex] );

			gl::color( Color::white() );
			vector<vec2> &points = mPolyLines[faceIndex].getPoints();
			for( size_t vertIndex = 0, size = points.size(); vertIndex < size; ++vertIndex ) {
				gl::drawSolidCircle( points[vertIndex], mHoverRadius / 2, circleSegments );

				if( ! mIsDragging && vertIndex == mActive.vert ) {
					// Draw a second outline to indicate selection
					gl::drawStrokedCircle( points[vertIndex], mHoverRadius, mHoverRadius / 2, circleSegments );
				}
			}

			if( mHover.hasVert() ) {
				drawMoveCursor( getHoverVertex() );
			} else if( mHover.hasEdge() ) {
				if( mIsDragging ) {
					drawMoveCursor( mCursorPosition );
				} else {
					drawAddCursor( mInsertPoint );
				}
			}
		} else if( faceIndex == mHover.face ) {
			gl::ScopedColor color( hover );
			gl::draw( mPolyLines[faceIndex] );
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
	gl::scale( vec2( 6 ) );

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
//	vec2 point( intersection.x, intersection.y );
//	onPlane = mIsSnapping ? snapToGrid( point, mGridSize ) : point;
    return true;
}

void prepareSettings( App::Settings *settings )
{
    settings->setHighDensityDisplayEnabled();
}
CINDER_APP( PolylineEditorApp, RendererGl( RendererGl::Options().msaa( 4 ) ), prepareSettings )
