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

	bool hasSelectedFace() { return mFaceSelected != mPolyLines.end(); };
	void deselectFace() { mFaceSelected = mPolyLines.end(); };
	void selectFirstFace() { mFaceSelected = mPolyLines.begin(); }
	void selectNextFace() {
		++mFaceSelected;
		if( ! hasSelectedFace() )
			selectFirstFace();
	}

	bool positionOnPlane( const vec2 &mouse, vec2 &onPlane );
	Iterator containedBy( const vec2 &onPlane );

	bool isAppending();

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
	if ( !this->isAppending() ) {
		mFaceHover = this->containedBy( mCursorPosition );
	}
}

void PolylineEditorApp::mouseDown( MouseEvent event )
{
	if( this->isAppending() ) {
		mPolyLines.back().push_back( mCursorPosition );
	} else {
		mFaceSelected = mFaceHover;
	}

	// Keel a recent value so when dragging starts the delta will be sane.
	mLastMouseDrag = event.getPos();
}

void PolylineEditorApp::mouseDrag( MouseEvent event ) {
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
			if( this->isAppending() )
				mPolyLines.pop_back();
			else
				deselectFace();
			break;
		case KeyEvent::KEY_RETURN:
			if( mPolyLines.size() )
				mPolyLines.back().setClosed();
			break;
		case KeyEvent::KEY_TAB:
			selectNextFace();
			break;
		case KeyEvent::KEY_n:
			if( this->isAppending() )
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

	gl::clear( Color( 0, 0, 0 ) );

	gl::ScopedMatrices matrixScope;
	gl::setMatrices( mEditCamera );

	if( mPolyLines.size() == 0 ) return;

	bool isAppending = this->isAppending();


	auto end = mPolyLines.end();
	if( isAppending )
		--end;

	// Draw completed shapes
	for ( auto p = mPolyLines.begin(); p != end; ++p ) {
		gl::ScopedColor color( ColorA( 1, 1, 1, 0.5 ) );
		gl::drawSolid( *p );
		if ( mFaceSelected == p ) {
			gl::ScopedColor color( selected );
			gl::draw( *p );
		} else if ( mFaceHover == p ) {
			gl::ScopedColor color( hover );
			gl::draw( *p );
		}
	}

	if ( isAppending ) {
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

bool PolylineEditorApp::positionOnPlane( const vec2 &mouse, vec2 &onPlane )
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


PolylineEditorApp::Iterator PolylineEditorApp::containedBy( const vec2 &onPlane )
{
	return std::find_if( mPolyLines.begin(), mPolyLines.end(),
		[&onPlane]( const PolyLine2f &p ) { return p.contains( onPlane ); } );
}

bool PolylineEditorApp::isAppending() {
	return mPolyLines.size() && ! mPolyLines.back().isClosed();
}

void prepareSettings( App::Settings *settings )
{
    settings->setHighDensityDisplayEnabled();
}
CINDER_APP( PolylineEditorApp, RendererGl, prepareSettings )
