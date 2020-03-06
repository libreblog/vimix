#include <cstring>
#include <thread>
#include <mutex>
#include <chrono>
#include <assert.h>

// Desktop OpenGL function loader
#include <glad/glad.h>  // Initialized with gladLoadGLLoader()

// Include glfw3.h after our OpenGL definitions
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

#ifdef APPLE
#include "osx/CocoaToolkit.h"
#define GLFW_EXPOSE_NATIVE_COCOA
#define GLFW_EXPOSE_NATIVE_NSGL
#else
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#endif
#include <GLFW/glfw3native.h>

#include <glm/ext/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale
#include <glm/ext/matrix_clip_space.hpp> // glm::perspective

// Include GStreamer
#include <gst/gl/gl.h>
#include <gst/gl/gstglcontext.h>

#ifdef GLFW_EXPOSE_NATIVE_COCOA
#include <gst/gl/cocoa/gstgldisplay_cocoa.h>
#endif
#ifdef GLFW_EXPOSE_NATIVE_GLX
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif

// standalone image loader
#include <stb_image.h>
#include <stb_image_write.h>

// vmix
#include "defines.h"
#include "Log.h"
#include "ResourceManager.h"
#include "SettingsManager.h"
#include "UserInterfaceManager.h"
#include "RenderingManager.h"

// Class statics
GLFWwindow* Rendering::window = nullptr;
std::string Rendering::glsl_version;
int Rendering::render_width = 0;
int Rendering::render_height = 0;
std::list<Rendering::RenderingCallback> Rendering::drawCallbacks;
Screenshot Rendering::window_screenshot;
bool Rendering::request_screenshot = false;

// local statics
static GstGLContext *global_gl_context = NULL;
static GstGLDisplay *global_display = NULL;
static guintptr global_window_handle = 0;


static void glfw_error_callback(int error, const char* description)
{
    std::string msg = "Glfw Error: " + std::string(description);
    UserInterface::Error(msg, APP_TITLE);
}

static void WindowRefreshCallback( GLFWwindow* window )
{
    Rendering::Draw();
}

glm::mat4 Rendering::Projection() {
    glm::mat4 projection = glm::ortho(-5.0, 5.0, -5.0, 5.0);
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.f, AspectRatio(), 1.f));

    return projection * scale;
}


bool Rendering::Init()
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()){
        UserInterface::Error("Failed to Initialize GLFW.");
        return false;
    }

    // Decide GL+GLSL versions GL 3.2 + GLSL 150
    glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#endif

    WindowSettings winset = Settings::application.windows.front();

    // Create window with graphics context
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window = glfwCreateWindow(winset.w, winset.h, winset.name.c_str(), NULL, NULL);
    if (window == NULL){
        UserInterface::Error("Failed to Create GLFW Window.");
        return false;
    }

    // Add application icon
    size_t fpsize = 0;
    const char *fp = Resource::getData("images/glmixer_256x256.png", &fpsize);
    if (fp != nullptr) {
        GLFWimage icon;
        icon.pixels = stbi_load_from_memory( (const stbi_uc*)fp, fpsize, &icon.width, &icon.height, nullptr, 4 );
        glfwSetWindowIcon( window, 1, &icon );
        free( icon.pixels );
    }

    glfwSetWindowPos(window, winset.x, winset.y);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync3
    glfwSetWindowRefreshCallback( window, WindowRefreshCallback );

    // Initialize OpenGL loader
    bool err = gladLoadGLLoader((GLADloadproc) glfwGetProcAddress) == 0;
    if (err) {
        UserInterface::Error("Failed to initialize OpenGL loader.");
        return false;
    }

    // show window
    glfwShowWindow(window);
    // restore fullscreen
    if (winset.fullscreen)
        ToggleFullscreen();

    // Rendering area (not necessarily same as window)
	glfwGetFramebufferSize(window, &render_width, &render_height);
	glViewport(0, 0, render_width, render_height);

    // Gstreamer link to context
    g_setenv ("GST_GL_API", "opengl3", FALSE);
    gst_init (NULL, NULL);

#if GST_GL_HAVE_PLATFORM_WGL
  context =
      gst_gl_context_new_wrapped (display, (guintptr) wglGetCurrentContext (),
      GST_GL_PLATFORM_WGL, GST_GL_API_OPENGL);
#elif GST_GL_HAVE_PLATFORM_CGL

// GstGLDisplay* display = gst_gl_display_new();
// guintptr context = gst_gl_context_get_current_gl_context(GST_GL_HAVE_PLATFORM_CGL);
// GstGLContext* gstcontext = gst_gl_context_new_wrapped(display, context, GST_GL_HAVE_PLATFORM_CGL, GST_GL_API_OPENGL);
// GstGLDisplay *gst_display;


   // GstGLDisplay *gst_display;

    //global_display = GST_GL_DISPLAY ( glfwGetCocoaMonitor(window) );

//GstGLDisplayCocoa *tmp = gst_gl_display_cocoa_new ();

    global_display = GST_GL_DISPLAY (gst_gl_display_cocoa_new ());

    //glfwMakeContextCurrent(window);

    global_gl_context = gst_gl_context_new_wrapped (global_display,
                                         (guintptr) 0,
                                         GST_GL_PLATFORM_CGL, GST_GL_API_OPENGL);

// id toto = glfwGetNSGLContext(window);

// g_warning("OpenGL Contexts  %ld %ld", (long int) glfwGetNSGLContext(window), (long int) CocoaToolkit::get_current_nsopengl_context() );

//     global_gl_context = gst_gl_context_new_wrapped (global_display,
//                                          (guintptr) glfwGetNSGLContext(window),
//                                          GST_GL_PLATFORM_CGL, GST_GL_API_OPENGL);

    // guintptr context;
    // context = gst_gl_context_get_current_gl_context (GST_GL_PLATFORM_CGL);

    // gst_display = gst_gl_display_new ();

    // global_gl_context = gst_gl_context_new_wrapped (GST_GL_DISPLAY (gst_display),
    //   context, GST_GL_PLATFORM_CGL, gst_gl_context_get_current_gl_api (GST_GL_PLATFORM_CGL, NULL, NULL));

    // global_display = GST_GL_DISPLAY ( gst_gl_context_get_display(global_gl_context) );

  //  global_window_handle =  (guintptr) glfwGetCocoaWindow(window);

#elif GST_GL_HAVE_PLATFORM_GLX

    //
    global_display = (GstGLDisplay*) gst_gl_display_x11_new_with_display( glfwGetX11Display() );

    global_gl_context = gst_gl_context_new_wrapped (global_display,
                                        (guintptr) glfwGetGLXContext(window),
                                        GST_GL_PLATFORM_GLX, GST_GL_API_OPENGL);

    global_window_handle =  (guintptr) glfwGetX11Window(window);

#endif

    // TODO : force GPU decoding

    // GstElementFactory *vdpauh264dec = gst_element_factory_find("vdpauh264dec");
    // if (vdpauh264dec)
    //     gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE(vdpauh264dec), GST_RANK_PRIMARY); // or GST_RANK_MARGINAL
    // GstElementFactory *vdpaumpeg4dec = gst_element_factory_find("vdpaumpeg4dec");
    // if (vdpaumpeg4dec)
    //     gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE(vdpaumpeg4dec), GST_RANK_PRIMARY);
    // GstElementFactory *vdpaumpegdec = gst_element_factory_find("vdpaumpegdec");
    // if (vdpaumpegdec)
    //     gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE(vdpaumpegdec), GST_RANK_PRIMARY);

    // file drop callback
    glfwSetDropCallback(window, Rendering::FileDropped);

    return true;
}

bool Rendering::isActive()
{
    return !glfwWindowShouldClose(window);
}


void Rendering::AddDrawCallback(RenderingCallback function)
{

    drawCallbacks.push_back(function);

}

void Rendering::Draw()
{
    if (Rendering::Begin() )
    {
        UserInterface::NewFrame();

		std::list<Rendering::RenderingCallback>::iterator iter;
		for (iter=drawCallbacks.begin(); iter != drawCallbacks.end(); iter++)
		{
            (*iter)();
        }

        UserInterface::Render();
        Rendering::End();
    }

    // no g_main_loop_run(loop) : update global GMainContext
    g_main_context_iteration(NULL, FALSE);

}

bool Rendering::Begin()
{
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    glfwPollEvents();

    glfwMakeContextCurrent(window);
    if( glfwGetWindowAttrib( window, GLFW_ICONIFIED ) )
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
        return false;
    }

    // handle window resize
	glfwGetFramebufferSize(window, &render_width, &render_height);

    // handle window resize
	glViewport(0, 0, render_width, render_height);

    // GL Colors
    glClearColor(0.2f, 0.2f, 0.2f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    return true;
}

void Rendering::End()
{
    glfwMakeContextCurrent(window);

    // perform screenshot if requested
    if (request_screenshot) {
        window_screenshot.CreateFromCaptureGL(0, 0, render_width, render_height);
        request_screenshot = false;
    }

    // swap GL buffers
    glfwSwapBuffers(window);
}


void Rendering::Terminate()
{
    // settings
    if ( !Settings::application.windows.front().fullscreen) {
        int x, y;
        glfwGetWindowPos(window, &x, &y);
        Settings::application.windows.front().x = x;
        Settings::application.windows.front().y = y;
        glfwGetWindowSize(window,&x, &y);
        Settings::application.windows.front().w = x;
        Settings::application.windows.front().h = y;
    }

    // close window
    glfwDestroyWindow(window);
    glfwTerminate();
}


void Rendering::Close()
{
    glfwSetWindowShouldClose(window, true);
}

void Rendering::ToggleFullscreen()
{
    // if in fullscreen mode
    if (glfwGetWindowMonitor(window) != nullptr) {
        // set to window mode
        glfwSetWindowMonitor( window, nullptr,  Settings::application.windows.front().x,
                                                Settings::application.windows.front().y,
                                                Settings::application.windows.front().w,
                                                Settings::application.windows.front().h, 0 );
        Settings::application.windows.front().fullscreen = false;
    }
    // not in fullscreen mode
    else {
        // remember window geometry
        int x, y;
        glfwGetWindowPos(window, &x, &y);
        Settings::application.windows.front().x = x;
        Settings::application.windows.front().y = y;
        glfwGetWindowSize(window,&x, &y);
        Settings::application.windows.front().w = x;
        Settings::application.windows.front().h = y;

        // select monitor
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode * mode = glfwGetVideoMode(monitor);

        // set to fullscreen mode
        glfwSetWindowMonitor( window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        Settings::application.windows.front().fullscreen = true;
    }

}

float Rendering::AspectRatio()
{
    return static_cast<float>(render_width) / static_cast<float>(render_height);
}


static GstBusSyncReply
bus_sync_handler (GstBus * bus, GstMessage * msg, gpointer user_data)
{
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_NEED_CONTEXT) {
        const gchar* contextType;
        gst_message_parse_context_type(msg, &contextType);

        if (!g_strcmp0(contextType, GST_GL_DISPLAY_CONTEXT_TYPE)) {
            GstContext *displayContext = gst_context_new(GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
            gst_context_set_gl_display(displayContext, global_display);
            gst_element_set_context(GST_ELEMENT(msg->src), displayContext);
            gst_context_unref (displayContext);

            g_info ("Managed %s\n", contextType);
        }
        if (!g_strcmp0(contextType, "gst.gl.app_context")) {
            GstContext *appContext = gst_context_new("gst.gl.app_context", TRUE);
            GstStructure* structure = gst_context_writable_structure(appContext);
            gst_structure_set(structure, "context", GST_TYPE_GL_CONTEXT, global_gl_context, nullptr);
            gst_element_set_context(GST_ELEMENT(msg->src), appContext);
            gst_context_unref (appContext);

            g_info ("Managed %s\n", contextType);
        }
    }

    gst_message_unref (msg);

    return GST_BUS_DROP;
}

void Rendering::LinkPipeline( GstPipeline *pipeline )
{
    // capture bus signals to force a unique opengl context for all GST elements
    GstBus* m_bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_set_sync_handler (m_bus, (GstBusSyncHandler) bus_sync_handler, pipeline, NULL);
    gst_object_unref (m_bus);


    // GstBus*  m_bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    // gst_bus_enable_sync_message_emission (m_bus);
    // g_signal_connect (m_bus, "sync-message", G_CALLBACK (bus_sync_handler), pipeline);
    // gst_object_unref (m_bus);
}

void Rendering::FileDropped(GLFWwindow* window, int path_count, const char* paths[])
{
    for (int i = 0; i < path_count; ++i) {

        Log::Info("Dropped file %s\n", paths[i]);
    }
}


Screenshot *Rendering::CurrentScreenshot()
{
    return &window_screenshot;
}

void Rendering::RequestScreenshot() 
{ 
    window_screenshot.Clear();
    request_screenshot = true; 
}

void Screenshot::CreateEmpty(int w, int h) 
{
    Clear();
    Width = w;
    Height = h;
    Data = (unsigned int*) malloc(Width * Height * 4 * sizeof(unsigned int));
    memset(Data, 0, Width * Height * 4);
}

void Screenshot::CreateFromCaptureGL(int x, int y, int w, int h) 
{
    Clear();
    Width = w;
    Height = h;
    Data = (unsigned int*) malloc(Width * Height * 4);

    // actual capture of frame buffer
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, Data);

    // make it usable
    RemoveAlpha();
    FlipVertical();
}

void Screenshot::SaveFile(const char* filename)
{
    if (Data)
        stbi_write_png(filename, Width, Height, 4, Data, Width * 4);
}

void Screenshot::RemoveAlpha()
{
    unsigned int* p = Data;
    int n = Width * Height;
    while (n-- > 0)
    {
        *p |= 0xFF000000;
        p++;
    }
}

void Screenshot::BlitTo(Screenshot* dst, int src_x, int src_y, int dst_x, int dst_y, int w, int h) const
{
    const Screenshot* src = this;
    assert(dst != src);
    assert(dst != NULL);
    assert(src_x >= 0 && src_y >= 0);
    assert(src_x + w <= src->Width);
    assert(src_y + h <= src->Height);
    assert(dst_x >= 0 && dst_y >= 0);
    assert(dst_x + w <= dst->Width);
    assert(dst_y + h <= dst->Height);
    for (int y = 0; y < h; y++)
        memcpy(dst->Data + dst_x + (dst_y + y) * dst->Width, src->Data + src_x + (src_y + y) * src->Width, w * 4);
}

void Screenshot::FlipVertical()
{
    int comp = 4;
    int stride = Width * comp;
    unsigned char* line_tmp = new unsigned char[stride];
    unsigned char* line_a = (unsigned char*)Data;
    unsigned char* line_b = (unsigned char*)Data + (stride * (Height - 1));
    while (line_a < line_b)
    {
        memcpy(line_tmp, line_a, stride);
        memcpy(line_a, line_b, stride);
        memcpy(line_b, line_tmp, stride);
        line_a += stride;
        line_b -= stride;
    }
    delete[] line_tmp;
}