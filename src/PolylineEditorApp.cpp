#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class PolylineEditorApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;
};

void PolylineEditorApp::setup()
{
}

void PolylineEditorApp::mouseDown( MouseEvent event )
{
}

void PolylineEditorApp::update()
{
}

void PolylineEditorApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) ); 
}

CINDER_APP( PolylineEditorApp, RendererGl )
