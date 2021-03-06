#include <unistd.h>
#include "init.h"
#include "gl.h"
#include "debug.h"
#include "../glx/hardext.h"
#include "../../version.h"
#include "../glx/streaming.h"
#if !defined(ANDROID) && !defined(AMIGAOS4)
#include <execinfo.h>
#endif

void gl_init();

globals4es_t globals4es;

#define SHUT(a) if(!globals4es.nobanner) a

#if defined(PANDORA) || defined(CHIP)
static void fast_math() {
  // enable Cortex A8 RunFast
   int v = 0;
   __asm__ __volatile__ (
     "vmrs %0, fpscr\n"
     "orr  %0, #((1<<25)|(1<<24))\n" // default NaN, flush-to-zero
     "vmsr fpscr, %0\n"
     //"vmrs %0, fpscr\n"
     : "=&r"(v));
}
#endif

#ifndef DEFAULT_ES
// forcing GLES 1.1 for now
#define DEFAULT_ES 1
#endif

void load_libs();
void glx_init();

#ifdef NO_INIT_CONSTRUCTOR
__attribute__((visibility("default")))
#else
__attribute__((constructor))
#endif
void initialize_gl4es() {
    // only init 1 time
    static int inited = 0;
    if(inited) return;
    inited = 1;
    // default init of globals
    memset(&globals4es, 0, sizeof(globals4es));
    globals4es.mergelist = 1;
    globals4es.queries = 1;
    globals4es.beginend = 1;
    // overides by env. variables
    char *env_nobanner = getenv("LIBGL_NOBANNER");
    if (env_nobanner && strcmp(env_nobanner, "1") == 0)
        globals4es.nobanner = 1;

	SHUT(LOGD("LIBGL: Initialising gl4es\n"));
	
    SHUT(LOGD("LIBGL: v%d.%d.%d built on %s %s\n", MAJOR, MINOR, REVISION, __DATE__, __TIME__));
    #define env(name, global, message)                    \
        char *env_##name = getenv(#name);                 \
        if (env_##name && strcmp(env_##name, "1") == 0) { \
            SHUT(LOGD("LIBGL: " message "\n"));         \
            global = true;                                \
        }

    env(LIBGL_XREFRESH, globals4es.xrefresh, "xrefresh will be called on cleanup");
    env(LIBGL_STACKTRACE, globals4es.stacktrace, "stacktrace will be printed on crash");

    char *env_fb = getenv("LIBGL_FB");
    if (env_fb && strcmp(env_fb, "1") == 0) {
            SHUT(LOGD("LIBGL: framebuffer output enabled\n"));
            globals4es.usefb = 1;
    }
    if (env_fb && strcmp(env_fb, "2") == 0) {
            SHUT(LOGD("LIBGL: using framebuffer + fbo\n"));
            globals4es.usefb = 1;
            globals4es.usefbo = 1;
    }
#ifndef NOX11
    if (env_fb && strcmp(env_fb, "3") == 0) {
            SHUT(LOGD("LIBGL: using pbuffer\n"));
            globals4es.usefb = 0;
            globals4es.usepbuffer = 1;
    }
#endif
    env(LIBGL_FPS, globals4es.showfps, "fps counter enabled");
#ifdef USE_FBIO
    env(LIBGL_VSYNC, globals4es.vsync, "vsync enabled");
#endif
#ifdef PANDORA
    char *env_gamma = getenv("LIBGL_GAMMA");
    if (env_gamma) {
        globals4es.gamma=atof(env_gamma);
        SHUT(LOGD("LIBGL: Set gamma to %.2f\n", globals4es.gamma));
    }
#endif
    env(LIBGL_NOBGRA, globals4es.nobgra, "Ignore BGRA texture capability");
    char *env_es = getenv("LIBGL_ES");
    if (env_es && strcmp(env_es, "1") == 0) {
            globals4es.es = 1;
    }
    if (env_es && strcmp(env_es, "2") == 0) {
            globals4es.es = 2;
    }
    // automatic ES backend selection
    if(globals4es.es==0)
        globals4es.es = DEFAULT_ES;

    char *env_gl = getenv("LIBGL_GL");
    if (env_gl && strcmp(env_gl, "15") == 0) {
            globals4es.gl = 15;
    }
    if (env_gl && strcmp(env_gl, "20") == 0) {
            globals4es.gl = 20;
    }
    if (env_gl && strcmp(env_gl, "21") == 0) {
            globals4es.gl = 21;
    }
    // automatic GL version selection
    if(globals4es.gl==0)
        globals4es.gl = (globals4es.es==1)?15:20;  // forcing GL 1.5 for es1.1 and GL 2.0 for es2.0

    SHUT(LOGD("LIBGL: Using GLES %s backend\n", (globals4es.es==1)?"1.1":"2.0"));

    load_libs();
    glx_init();

#ifdef NOEGL
    int gl4es_notest = 1;
#else
    int gl4es_notest = 0;
    char *env_notest = getenv("LIBGL_NOTEST");
    if (env_notest && strcmp(env_notest, "1") == 0) {
		gl4es_notest = 1;
    }
#endif
    env(LIBGL_NOHIGHP, globals4es.nohighp, "Do not use HIGHP in fragment shader even if detected");

    GetHardwareExtensions(gl4es_notest);
    gl_init();

    env(LIBGL_RECYCLEFBO, globals4es.recyclefbo, "Recycling of FBO enabled");
    // Texture hacks
    char *env_mipmap = getenv("LIBGL_MIPMAP");
    if (env_mipmap && strcmp(env_mipmap, "1") == 0) {
        globals4es.automipmap = 1;
        SHUT(LOGD("LIBGL: AutoMipMap forced\n"));
    }
    if (env_mipmap && strcmp(env_mipmap, "2") == 0) {
        globals4es.automipmap = 2;
        SHUT(LOGD("LIBGL: guess AutoMipMap\n"));
    }
    if (env_mipmap && strcmp(env_mipmap, "3") == 0) {
        globals4es.automipmap = 3;
        SHUT(LOGD("LIBGL: ignore MipMap\n"));
    }
    if (env_mipmap && strcmp(env_mipmap, "4") == 0) {
        globals4es.automipmap = 4;
        SHUT(LOGD("LIBGL: ignore AutoMipMap on non-squared textures\n"));
    }
    char *env_texcopy = getenv("LIBGL_TEXCOPY");
    if (env_texcopy && strcmp(env_texcopy, "1") == 0) {
        globals4es.texcopydata = 1;
        SHUT(LOGD("LIBGL: Texture copy enabled\n"));
    }
    char *env_shrink = getenv("LIBGL_SHRINK");
    if(env_shrink) {
        if (env_shrink && strcmp(env_shrink, "10") == 0) {
            globals4es.texshrink = 10;
            SHUT(LOGD("LIBGL: Texture shink, mode 10 selected (advertise 8192 max texture size, but >2048 are quadshrinked and > 512 are shrinked), but not for empty texture\n"));
        }
        else if (env_shrink && strcmp(env_shrink, "1") == 0) {
            globals4es.texshrink = 1;
            SHUT(LOGD("LIBGL: Texture shink, mode 1 selected (everything / 2)\n"));
        }
        else if (env_shrink && strcmp(env_shrink, "2") == 0) {
            globals4es.texshrink = 2;
            SHUT(LOGD("LIBGL: Texture shink, mode 2 selected (only > 512 /2 )\n"));
        }
        else if (env_shrink && strcmp(env_shrink, "3") == 0) {
            globals4es.texshrink = 3;
            SHUT(LOGD("LIBGL: Texture shink, mode 3 selected (only > 256 /2 )\n"));
        }
        else if (env_shrink && strcmp(env_shrink, "4") == 0) {
            globals4es.texshrink = 4;
            SHUT(LOGD("LIBGL: Texture shink, mode 4 selected (only > 256 /2, >=1024 /4 )\n"));
        }
        else if (env_shrink && strcmp(env_shrink, "5") == 0) {
            globals4es.texshrink = 5;
            SHUT(LOGD("LIBGL: Texture shink, mode 5 selected (every > 256 is downscaled to 256 ), but not for empty texture\n"));
        }
        else if (env_shrink && strcmp(env_shrink, "6") == 0) {
            globals4es.texshrink = 6;
            SHUT(LOGD("LIBGL: Texture shink, mode 6 selected (only > 128 /2, >=512 is downscaled to 256 ), but not for empty texture\n"));
        }
        else if (env_shrink && strcmp(env_shrink, "7") == 0) {
            globals4es.texshrink = 7;
            SHUT(LOGD("LIBGL: Texture shink, mode 7 selected (only > 512 /2 ), but not for empty texture\n"));
        }
        else if (env_shrink && strcmp(env_shrink, "8") == 0) {
            globals4es.texshrink = 8;
            SHUT(LOGD("LIBGL: Texture shink, mode 8 selected (advertise 8192 max texture size, but >2048 are shrinked to 2048)\n"));
        }
        else if (env_shrink && strcmp(env_shrink, "9") == 0) {
            globals4es.texshrink = 9;
            SHUT(LOGD("LIBGL: Texture shink, mode 9 selected (advertise 8192 max texture size, but >4096 are quadshrinked and > 512 are shrinked), but not for empty texture\n"));
        }
    }
    if(globals4es.texshrink>7) hardext.maxsize*=4;

    char *env_dump = getenv("LIBGL_TEXDUMP");
    if (env_dump && strcmp(env_dump, "1") == 0) {
        globals4es.texdump = 1;
        SHUT(LOGD("LIBGL: Texture dump enabled\n"));
    }
    char *env_alpha = getenv("LIBGL_ALPHAHACK");
    if (env_alpha && strcmp(env_alpha, "1") == 0) {
        globals4es.alphahack = 1;
        SHUT(LOGD("LIBGL: Alpha Hack enabled\n"));
    }
#ifdef TEXSTREAM
    char *env_stream = getenv("LIBGL_STREAM");
    if (env_stream && strcmp(env_stream, "1") == 0) {
        globals4es.texstream = InitStreamingCache();
        SHUT(LOGD("LIBGL: Streaming texture %s\n",(globals4es.texstream)?"enabled":"not available"));
        //FreeStreamed(AddStreamed(1024, 512, 0));
    }
    if (env_stream && strcmp(env_stream, "2") == 0) {
        globals4es.texstream = InitStreamingCache()?2:0;
        SHUT(LOGD("LIBGL: Streaming texture %s\n",(globals4es.texstream)?"forced":"not available"));
        //FreeStreamed(AddStreamed(1024, 512, 0));
    }
#endif
    char *env_copy = getenv("LIBGL_COPY");
    if (env_copy && strcmp(env_copy, "1") == 0) {
        SHUT(LOGD("LIBGL: No glCopyTexImage2D / glCopyTexSubImage2D hack\n"));
        globals4es.copytex = 1;
    }
    char *env_lumalpha = getenv("LIBGL_NOLUMALPHA");
    if (env_lumalpha && strcmp(env_lumalpha, "1") == 0) {
        globals4es.nolumalpha = 1;
        SHUT(LOGD("LIBGL: GL_LUMINANCE_ALPHA hardware support disabled\n"));
    }

    env(LIBGL_BLENDHACK, globals4es.blendhack, "Change Blend GL_SRC_ALPHA, GL_ONE to GL_ONE, GL_ONE");
    env(LIBGL_BLENDCOLOR, globals4es.blendcolor, "Export a (faked) glBlendColor");
    env(LIBGL_NOERROR, globals4es.noerror, "glGetError() always return GL_NOERROR");
    env(LIBGL_SILENTSTUB, globals4es.silentstub, "Stub/non present functions are not printed");
    env(LIBGL_VABGRA, globals4es.vabgra, "Export GL_ARB_vertex_array_bgra extension");

    char *env_version = getenv("LIBGL_VERSION");
    if (env_version) {
        SHUT(LOGD("LIBGL: Overide version string with \"%s\" (should be in the form of \"1.x\")\n", env_version));
    }
    if(env_version) {
        snprintf(globals4es.version, 49, "%s gl4es wrapper", env_version);
        SHUT(LOGD("LIBGL: Targeting OpenGL %s\n", env_version));
    } else {
        snprintf(globals4es.version, 49, "%d.%d gl4es wrapper", globals4es.gl/10, globals4es.gl%10);
        SHUT(LOGD("LIBGL: Targeting OpenGL %d.%d\n", globals4es.gl/10, globals4es.gl%10));
    }

    char *env_srgb = getenv("LIBGL_SRGB");
    if (env_srgb && strcmp(env_srgb, "1") == 0 && hardext.srgb) {
        globals4es.glx_surface_srgb = 2;
        SHUT(LOGD("LIBGL: enabling sRGB support\n"));
    }
    char *env_fastmath = getenv("LIBGL_FASTMATH");
    if (env_fastmath && strcmp(env_fastmath, "1") == 0) {
#if defined(PANDORA) || defined(CHIP)
        SHUT(LOGD("LIBGL: Enable FastMath for cortex-a8\n"));
        fast_math();
#else
        SHUT(LOGD("LIBGL: No FastMath on this platform\n"));
#endif
    }
    char *env_npot = getenv("LIBGL_NPOT");
    globals4es.npot = hardext.npot;
    if (env_npot && strcmp(env_npot, "1") == 0 && globals4es.npot<1) {
		globals4es.npot = 1;
		SHUT(LOGD("LIBGL: Expose limited NPOT extension\n"));
	}
    if (env_npot && strcmp(env_npot, "2") == 0 && globals4es.npot<2) {
		globals4es.npot = 2;
		SHUT(LOGD("LIBGL: Expose GL_ARB_texture_non_power_of_two extension\n"));
	}
   char *env_queries = getenv("LIBGL_GLQUERIES");
    if (env_queries && strcmp(env_queries, "0") == 0) {
        globals4es.queries = 0;
        SHUT(LOGD("LIBGL: Dont't expose fake glQueries functions\n"));
    }
    char *env_nodownsampling = getenv("LIBGL_NODOWNSAMPLING");
    if (env_nodownsampling && strcmp(env_nodownsampling, "1") == 0) {
        globals4es.nodownsampling = 1;
        SHUT(LOGD("LIBGL: No downsampling of DXTc textures\n"));
    }

    env(LIBGL_NOTEXMAT, globals4es.texmat, "Don't handle Texture Matrice internaly");
    env(LIBGL_NOVAOCACHE, globals4es.novaocache, "Don't use VAO cache");
    
    char *env_beginend = getenv("LIBGL_BEGINEND");
    if(env_beginend) {
        if (strcmp(env_beginend, "0") == 0) {
                globals4es.beginend = 0;
                globals4es.mergelist = 0;
                SHUT(LOGD("LIBGL: Don't try to merge subsequent glBegin/glEnd blocks\n"));
        } 
        if (strcmp(env_beginend, "1") == 0) {
                globals4es.beginend = 1;
                SHUT(LOGD("LIBGL: Try to merge subsequent glBegin/glEnd blocks, even if there is a glColor / glNormal in between\n"));
        } 
        if (strcmp(env_beginend, "2") == 0) {
                globals4es.beginend = 2;
                SHUT(LOGD("LIBGL: Try hard to merge subsequent glBegin/glEnd blocks, even if there is a glColor / glNormal or Matrix operations in between\n"));
        } 
    }
    env(LIBGL_AVOID16BITS, globals4es.avoid16bits, "Avoid 16bits textures");

    globals4es.avoid24bits = 0;
    char *env_avoid24bits = getenv("LIBGL_AVOID24BITS");
    if(env_avoid24bits) {
        if (strcmp(env_avoid24bits, "0") == 0) {
            globals4es.avoid24bits = 0;
            SHUT(LOGD("LIBGL: Don't try to avoid 24bits textures\n"));
        } 
        if (strcmp(env_avoid24bits, "1") == 0) {
            globals4es.avoid24bits = 2;
            SHUT(LOGD("LIBGL: Avoid 24bits textures\n"));
        } 
    }
    env(LIBGL_POTFRAMEBUFFER, globals4es.potframebuffer, "Force framebuffers to be on POT size");

    char *env_forcenpot = getenv("LIBGL_FORCENPOT");
    if ((env_forcenpot && strcmp(env_forcenpot,"0") == 0) && (hardext.esversion==2 && hardext.npot==1)) {
        SHUT(LOGD("LIBGL: Not forcing NPOT support\n"));
    } else
    if ((env_forcenpot && strcmp(env_forcenpot,"1") == 0) || (hardext.esversion==2 && hardext.npot==1)) {
        if(hardext.npot==2) {
            SHUT(LOGD("LIBGL: NPOT texture handled in hardware\n"));
        } else if(hardext.npot==1) {
            globals4es.forcenpot = 1;
            SHUT(LOGD("LIBGL: Forcing NPOT support by disabling MIPMAP support for NPOT textures \n"));
        } else {
            SHUT(LOGD("LIBGL: WARNING, No Limited or Full NPOT support in hardware, Forcing NPOT have no effect!\n"));
        }
    }
    globals4es.batch = 0;
    char *env_batch = getenv("LIBGL_BATCH");
    int tmp = 0;
    if(env_batch && sscanf(env_batch, "%d", &tmp)==1) {
        globals4es.batch = tmp;
        if(tmp==0) {
            SHUT(LOGD("LIBGL: Not trying to batch small subsequent glDrawXXXX\n"));
        } else {
            SHUT(LOGD("LIBGL: Trying to batch subsequent glDrawXXXX of size < %d vertices\n", tmp*10));
        }
    }

    globals4es.usevbo = 0;
    char *env_usevbo = getenv("LIBGL_USEVBO");
    /*if(env_usevbo && strcmp(env_usevbo,"0") == 0) {
        SHUT(LOGD("LIBGL: Use of VBO disabled\n"));
        globals4es.usevbo = 0;
    } else*/ if(env_usevbo && strcmp(env_usevbo,"1") == 0) {
        globals4es.usevbo = 1;
    }
    if(globals4es.usevbo) {
        SHUT(LOGD("LIBGL: VBO used (in a few cases)\n"));
    }

    globals4es.fbomakecurrent = 0;
    if((hardext.vendor & VEND_ARM) || (globals4es.usefb))
        globals4es.fbomakecurrent = 1;
    char *env_fbomakecurrent = getenv("LIBGL_FBOMAKECURRENT");
    if(globals4es.fbomakecurrent && env_fbomakecurrent && !strcmp(env_fbomakecurrent, "0")) {
        globals4es.fbomakecurrent = 0;
        SHUT(LOGD("LIBGL: glXMakeCurrent FBO workaround disabled\n"));
    }
    if(env_fbomakecurrent && !strcmp(env_fbomakecurrent, "1"))
        globals4es.fbomakecurrent = 1;
    if(globals4es.fbomakecurrent) {
        SHUT(LOGD("LIBGL: glXMakeCurrent FBO workaround enabled\n"));
    }

    env(LIBGL_COMMENTS, globals4es.comments, "Keep comments in converted Shaders");
    
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))!= NULL)
        SHUT(LOGD("LIBGL: Current folder is:%s\n", cwd));
}
