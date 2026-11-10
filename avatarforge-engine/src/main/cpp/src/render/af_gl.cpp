/*
 * ============================================================================================
 *  AvatarForge :: af_gl.cpp  --  the fallback renderer: EGL + OpenGL ES 3.1
 * ============================================================================================
 *
 *  Same shading model, same shader source, same frame structure as the Vulkan backend - this file
 *  is the other half of "runs on any Android device the app installs on":
 *
 *      EGL window surface -> MSAA HDR framebuffer -> resolve to HDR texture
 *                         -> bloom chain (down + additive up)
 *                         -> composite (exposure, ACES, vignette, dither, sRGB) -> window
 *      plus a depth-only shadow pass with a comparison-sampler map, exactly as in af_vk.cpp.
 *
 *  Differences from the Vulkan path, all of them API-mandated and commented where they matter:
 *      * objects are bound to the context instead of recorded into command buffers
 *      * multisampling is a framebuffer property here, so the HDR attachment is a multisampled
 *        renderbuffer that is blitted into a texture (GLES has no resolve-at-end-of-pass)
 *      * the Y direction is not flipped: GL's clip space already matches the shader's expectation
 *        (AF_CLIP_Y is +1 in the ES dialect), and the depth range is [-1,1] rather than [0,1]
 *      * glGenerateMipmap is used instead of a blit chain
 *
 *  The backend refuses to start below OpenGL ES 3.1 rather than silently degrading: the app's
 *  contract says the renderer needs a 3.1-class GPU, and pretending otherwise would ship a
 *  different (worse) image than the one the user was promised.
 */
#include "af_render.h"
#include "af_render_common.h"
#include "af_environment.h"
#include "af_shaders.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>            /* EGL_OPENGL_ES3_BIT_KHR */
#include <GLES3/gl31.h>
#include <android/native_window.h>
#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace af {
namespace {

#define AF_GL_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "AvatarForge", __VA_ARGS__)
#define AF_GL_LOGW(...) __android_log_print(ANDROID_LOG_WARN, "AvatarForge", __VA_ARGS__)
#define AF_GL_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AvatarForge", __VA_ARGS__)

/* TEMPORARY: bypass avatar PBR/material/texture shading to isolate the GLES camera path. */
static constexpr bool kDebugMagentaAvatar = true;

static constexpr const char* kDebugMagentaVertexShader = R"AFGLSL(#version 310 es
precision highp float;
layout(location = 0) in vec3 aPos;
layout(std140) uniform CameraBlock {
    mat4 uViewProj;
};
void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)AFGLSL";

static constexpr const char* kDebugMagentaFragmentShader = R"AFGLSL(#version 310 es
precision highp float;
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(1.0, 0.0, 1.0, 1.0);
}
)AFGLSL";

void logVertexAttribute(GLuint index, GLint expectedComponents, size_t expectedOffset) {
    GLint enabled = 0, size = 0, type = 0, normalized = 0, stride = 0, buffer = 0;
    void* pointer = nullptr;
    glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
    glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
    glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
    glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);
    glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
    glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer);
    glGetVertexAttribPointerv(index, GL_VERTEX_ATTRIB_ARRAY_POINTER, &pointer);
    AF_GL_LOGI("VAO attr%u enabled=%d size=%d type=0x%x normalized=%d stride=%d buffer=%d offset=%zu expected(size=%d type=GL_FLOAT stride=%zu offset=%zu)",
               index, enabled, size, type, normalized, stride, buffer,
               reinterpret_cast<uintptr_t>(pointer), expectedComponents, sizeof(af_vertex), expectedOffset);
    if (!enabled || size != expectedComponents || type != GL_FLOAT || stride != sizeof(af_vertex) ||
        reinterpret_cast<uintptr_t>(pointer) != expectedOffset || buffer == 0) {
        AF_GL_LOGE("VAO attribute %u does not match af_vertex layout", index);
    }
}

bool compileShader(GLenum type, const char* source, const char* label, GLuint* out, std::string* error) {
    const GLuint shader = glCreateShader(type);
    if (!shader) {
        if (error) *error = std::string("glCreateShader failed for ") + label;
        return false;
    }
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(size_t(len > 1 ? len : 1), '\0');
        glGetShaderInfoLog(shader, len, nullptr, &log[0]);
        if (error) *error = std::string(label) + ": " + log;
        glDeleteShader(shader);
        return false;
    }
    *out = shader;
    return true;
}

class GlRenderer final : public Renderer {
public:
    bool init(ANativeWindow* window, const RendererOptions& options, std::string* error);
    ~GlRenderer() override;

    bool resize(int width, int height) override;
    void attachEngine(af_engine* engine) override;
    void invalidateMesh() override;
    void setCamera(const CameraState& camera) override { camera_ = camera; }
    void setOptions(const RendererOptions& options) override;
    RendererOptions options() const override { return options_; }
    void renderFrame(float deltaSeconds) override;
    void setSkeletonVisible(bool visible) override { skeletonVisible_ = visible; }
    void setFlatShading(bool flat) override { flatShading_ = flat; }
    void setHighlightRegion(int region) override { highlightRegion_ = region; }
    void setClip(int clip, float speed, bool loop) override;
    int currentClip() const override { return clip_; }
    float clipTime() const override { return clipTime_; }
    void waitIdle() override { glFinish(); }
    RendererStats stats() const override { return stats_; }
    std::vector<uint8_t> readbackPixels() const override { return verificationPixels_; }
    RendererInfo info() const override { return info_; }

private:
    bool createContext(ANativeWindow* window, std::string* error);
    bool createPrograms(std::string* error);
    bool createTargets(std::string* error);
    bool createAtlas(std::string* error);
    void uploadAtlas(std::string* error);
    void ensureGeometryBuffers();
    void rebuildMaterialBlocks();
    void buildSkeletonLines();
    void updateScene(float deltaSeconds);
    void updateAnimation(float deltaSeconds);
    void drawShadowPass();
    void drawForwardPass();
    void drawBloom();
    void drawComposite();
    void destroyTargets();
    void destroyAll();
    bool makeFramebufferWithTexture(GLuint* fbo, GLuint* texture, int w, int h, GLenum internalFormat,
                                    GLenum format, GLenum type, bool depth);

    GLuint linkProgram(const char* vsName, const char* fsName, std::string* error);

    /* ---- EGL / context ---- */
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLConfig config_ = nullptr;
    int surfaceWidth_ = 0;
    int surfaceHeight_ = 0;
    uint32_t uboAlignment_ = 256;

    /* ---- programs and uniform locations ---- */
    GLuint forwardProgram_ = 0, shadowProgram_ = 0, groundProgram_ = 0, lineProgram_ = 0;
    GLuint debugMagentaProgram_ = 0;
    GLuint bloomDownProgram_ = 0, bloomUpProgram_ = 0, compositeProgram_ = 0;
    GLuint avatarVao_ = 0, groundVao_ = 0, emptyVao_ = 0, lineVao_ = 0;

    /* ---- targets ---- */
    GLuint hdrFbo_ = 0, hdrTexture_ = 0;              /* resolved HDR image           */
    GLuint msaaFbo_ = 0, msaaColorRb_ = 0, msaaDepthRb_ = 0;
    GLuint shadowFbo_ = 0, shadowTexture_ = 0;
    GLuint bloomFbo_[5] = {0, 0, 0, 0, 0};
    GLuint bloomTexture_[5] = {0, 0, 0, 0, 0};
    int bloomWidth_[5] = {0, 0, 0, 0, 0};
    int bloomHeight_[5] = {0, 0, 0, 0, 0};
    int bloomLevels_ = 0;
    GLenum hdrInternalFormat_ = GL_RGBA16F;

    /* ---- buffers ---- */
    GLuint cameraUbo_ = 0, postUbo_ = 0, materialUbo_ = 0;
    GLuint vertexVbo_ = 0, indexIbo_ = 0, groundVbo_ = 0, lineVbo_ = 0;
    GLuint atlasTexture_[AF_TEX_COUNT] = {0, 0, 0, 0, 0};
    bool atlasUploaded_ = false;
    GLint64 vertexCapacity_ = 0, indexCapacity_ = 0, materialCapacity_ = 0, lineCapacity_ = 0;

    /* ---- uniform locations ---- */
    struct Uniforms {
        GLint baseColor = -1, normal = -1, metalRough = -1, occlusion = -1, subsurface = -1, shadow = -1;
        GLint source = -1, bloom = -1;
    } uForward_{}, uShadow_{}, uGround_{}, uLine_{}, uPost_{};

    /* ---- state ---- */
    RendererOptions options_{};
    RendererOptions activeTargetOptions_{};  /* last target set that completed successfully */
    RendererInfo info_{};
    RendererStats stats_{};
    std::vector<uint8_t> verificationPixels_;
    af_engine* engine_ = nullptr;
    CameraState camera_{};
    FrameScene scene_{};
    EnvironmentParams env_{};
    ShGlobal sh_{};
    bool meshDirty_ = true;
    int highlightRegion_ = -1;
    bool skeletonVisible_ = false;
    bool flatShading_ = false;
    int clip_ = -1;
    float clipSpeed_ = 1.0f;
    bool clipLoop_ = true;
    float clipTime_ = 0.0f;
    int32_t vertexCount_ = 0;
    int32_t indexCount_ = 0;
    uint32_t materialStride_ = 256;
    int materialCount_ = 0;
    uint32_t lineVertexCount_ = 0;
    int drawCalls_ = 0;
    int triangles_ = 0;
    bool targetsDirty_ = false;
    bool contextLost_ = false;
    bool geometryValid_ = false;
    bool vertexDumpLogged_ = false;
    bool cameraDumpLogged_ = false;
    bool drawDumpLogged_ = false;
    float cameraLogAccumulatorSeconds_ = 0.0f;
    GLenum lastDrawError_ = GL_NO_ERROR;
};

/* ============================================================================================
 *  Context
 * ============================================================================================ */
bool GlRenderer::createContext(ANativeWindow* window, std::string* error) {
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) { *error = "eglGetDisplay failed"; return false; }
    if (!eglInitialize(display_, nullptr, nullptr)) { *error = "eglInitialize failed"; return false; }

    /* Require ES 3.1: ES3_BIT_KHR with a 3.x context, plus a depth buffer for the shadow pass and
       a window surface that EGL itself considers RGB-capable. Multisampling is done with
       renderbuffers (the window surface is only ever blitted to), so no MSAA config is needed. */
    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,   /* ES 3.x renderable */
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };
    EGLint configCount = 0;
    if (!eglChooseConfig(display_, configAttribs, &config_, 1, &configCount) || configCount == 0) {
        *error = "no EGL config with an ES3 renderable type and a 24-bit depth buffer";
        return false;
    }

    const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, contextAttribs);
    if (context_ == EGL_NO_CONTEXT) { *error = "eglCreateContext(ES3) failed"; return false; }

    surface_ = eglCreateWindowSurface(display_, config_, window, nullptr);
    if (surface_ == EGL_NO_SURFACE) { *error = "eglCreateWindowSurface failed"; return false; }

    if (!eglMakeCurrent(display_, surface_, surface_, context_)) {
        *error = "eglMakeCurrent failed";
        return false;
    }
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    eglQuerySurface(display_, surface_, EGL_WIDTH, &surfaceWidth_);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &surfaceHeight_);

    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* rendererName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    if (!version) { *error = "glGetString(GL_VERSION) returned null"; return false; }

    int major = 0, minor = 0;
    std::sscanf(version, "OpenGL ES %d.%d", &major, &minor);
    if (major * 10 + minor < 31) {
        *error = std::string("OpenGL ES 3.1 is required, this device reports: ") + version;
        return false;
    }

    GLint align = 256;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &align);
    uboAlignment_ = uint32_t(align > 0 ? align : 256);

    /* Floating-point colour attachments: ES 3.2 has them in core, ES 3.0/3.1 need the extension. */
    bool floatColour = (major > 3 || (major == 3 && minor >= 2));
    if (!floatColour) {
        const char* ext = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        floatColour = ext && std::strstr(ext, "GL_EXT_color_buffer_float") != nullptr;
    }
    hdrInternalFormat_ = floatColour ? GL_RGBA16F : GL_RGBA8;

    info_.backend = "gles31";
    info_.deviceName = rendererName ? rendererName : "unknown GPU";
    info_.apiVersion = version;
    info_.driver = vendor ? vendor : "unknown vendor";
    info_.supportsFloatHdr = floatColour;
    info_.supportsMsaa = true;
    info_.supportsShadows = true;
    info_.supportsAnisotropy = true;   /* queried properly below; ES 3.0 has anisotropy via the
                                          same enum, and the sampler clamps to the driver limit */
    return true;
}

/* ============================================================================================
 *  Programs
 * ============================================================================================ */
GLuint GlRenderer::linkProgram(const char* vsName, const char* fsName, std::string* error) {
    const AfShaderGl* vs = afShaderGl(vsName, AF_STAGE_VERT);
    const AfShaderGl* fs = afShaderGl(fsName, AF_STAGE_FRAG);
    if (!vs || !fs) {
        *error = std::string("shader source missing for ") + vsName;
        return 0;
    }
    GLuint vsobj = 0, fsobj = 0;
    if (!compileShader(GL_VERTEX_SHADER, vs->source, vsName, &vsobj, error)) return 0;
    if (!compileShader(GL_FRAGMENT_SHADER, fs->source, fsName, &fsobj, error)) {
        glDeleteShader(vsobj);
        return 0;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vsobj);
    glAttachShader(program, fsobj);
    glLinkProgram(program);
    glDeleteShader(vsobj);
    glDeleteShader(fsobj);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::string log(size_t(len > 1 ? len : 1), '\0');
        glGetProgramInfoLog(program, len, nullptr, &log[0]);
        *error = std::string("link ") + vsName + "/" + fsName + ": " + log;
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

bool GlRenderer::createPrograms(std::string* error) {
    if (!(forwardProgram_ = linkProgram("forward", "forward", error))) return false;
    if (!(shadowProgram_ = linkProgram("shadow", "shadow", error))) return false;
    if (!(groundProgram_ = linkProgram("ground", "ground", error))) return false;
    if (!(lineProgram_ = linkProgram("line", "line", error))) return false;
    if (!(bloomDownProgram_ = linkProgram("fullscreen", "bloom_down", error))) return false;
    if (!(bloomUpProgram_ = linkProgram("fullscreen", "bloom_up", error))) return false;
    if (!(compositeProgram_ = linkProgram("fullscreen", "composite", error))) return false;

    /* Temporary unlit avatar shader. Position attribute and CameraBlock binding match the PBR path. */
    {
        GLuint vs = 0;
        GLuint fs = 0;
        if (!compileShader(GL_VERTEX_SHADER, kDebugMagentaVertexShader,
                           "debug_magenta_vertex", &vs, error)) return false;
        if (!compileShader(GL_FRAGMENT_SHADER, kDebugMagentaFragmentShader,
                           "debug_magenta_fragment", &fs, error)) {
            glDeleteShader(vs);
            return false;
        }
        debugMagentaProgram_ = glCreateProgram();
        if (!debugMagentaProgram_) {
            glDeleteShader(vs);
            glDeleteShader(fs);
            if (error) *error = "glCreateProgram failed for debug magenta shader";
            return false;
        }
        glAttachShader(debugMagentaProgram_, vs);
        glAttachShader(debugMagentaProgram_, fs);
        glLinkProgram(debugMagentaProgram_);
        glDeleteShader(vs);
        glDeleteShader(fs);

        GLint linked = GL_FALSE;
        glGetProgramiv(debugMagentaProgram_, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            GLint logLength = 0;
            glGetProgramiv(debugMagentaProgram_, GL_INFO_LOG_LENGTH, &logLength);
            std::string log(static_cast<size_t>(logLength > 1 ? logLength : 1), '\0');
            glGetProgramInfoLog(debugMagentaProgram_, logLength, nullptr, log.data());
            if (error) *error = "Debug magenta shader link failed: " + log;
            glDeleteProgram(debugMagentaProgram_);
            debugMagentaProgram_ = 0;
            return false;
        }
        const GLuint cameraBlock = glGetUniformBlockIndex(debugMagentaProgram_, "CameraBlock");
        if (cameraBlock == GL_INVALID_INDEX) {
            if (error) *error = "Debug magenta shader has no CameraBlock";
            glDeleteProgram(debugMagentaProgram_);
            debugMagentaProgram_ = 0;
            return false;
        }
        glUniformBlockBinding(debugMagentaProgram_, cameraBlock, 0);
        AF_GL_LOGW("DEBUG_MAGENTA shader linked program=%u CameraBlock=%u",
                   debugMagentaProgram_, cameraBlock);
    }

    /* Uniform blocks are bound by index rather than relying on the layout(binding=) qualifier,
       which ES 3.1 accepts but not every driver implements identically. */
    const GLuint programs[8] = {forwardProgram_, shadowProgram_, groundProgram_, lineProgram_,
                                bloomDownProgram_, bloomUpProgram_, compositeProgram_,
                                debugMagentaProgram_};
    bool hasCameraBlock = false;
    for (GLuint p : programs) {
        const GLuint cameraIndex = glGetUniformBlockIndex(p, "CameraBlock");
        if (cameraIndex != GL_INVALID_INDEX) {
            glUniformBlockBinding(p, cameraIndex, 0);
            hasCameraBlock = true;
        }
        const GLuint materialIndex = glGetUniformBlockIndex(p, "MaterialBlock");
        if (materialIndex != GL_INVALID_INDEX) glUniformBlockBinding(p, materialIndex, 1);
        const GLuint postIndex = glGetUniformBlockIndex(p, "PostBlock");
        if (postIndex != GL_INVALID_INDEX) glUniformBlockBinding(p, postIndex, 2);
    }
    if (!hasCameraBlock) {
        *error = "no program declared the CameraBlock uniform block (shader/ABI mismatch)";
        return false;
    }

    /* Sampler units. The GLES dialect of common.glsl declares the samplers without a binding
       qualifier, so the units are assigned here, next to the code that binds them. */
    auto unit = [](GLuint p, const char* name, GLint value) {
        const GLint loc = glGetUniformLocation(p, name);
        if (loc >= 0) glUniform1i(loc, value);
    };
    unit(forwardProgram_, "uBaseColorTex", 0);
    unit(forwardProgram_, "uNormalTex", 1);
    unit(forwardProgram_, "uMetalRoughTex", 2);
    unit(forwardProgram_, "uOcclusionTex", 3);
    unit(forwardProgram_, "uSubsurfaceTex", 4);
    unit(forwardProgram_, "uShadowMap", 5);
    unit(groundProgram_, "uShadowMap", 5);
    unit(bloomDownProgram_, "uSourceTex", 6);
    unit(bloomUpProgram_, "uSourceTex", 6);
    unit(compositeProgram_, "uSourceTex", 6);
    unit(compositeProgram_, "uBloomTex", 7);
    unit(shadowProgram_, "uBaseColorTex", 0);   /* present only to keep the block uniform */

    uForward_.baseColor = glGetUniformLocation(forwardProgram_, "uBaseColorTex");
    uForward_.normal = glGetUniformLocation(forwardProgram_, "uNormalTex");
    uForward_.metalRough = glGetUniformLocation(forwardProgram_, "uMetalRoughTex");
    uForward_.occlusion = glGetUniformLocation(forwardProgram_, "uOcclusionTex");
    uForward_.subsurface = glGetUniformLocation(forwardProgram_, "uSubsurfaceTex");
    uPost_.source = glGetUniformLocation(bloomDownProgram_, "uSourceTex");
    return true;
}

/* ============================================================================================
 *  Targets
 * ============================================================================================ */
bool GlRenderer::makeFramebufferWithTexture(GLuint* fbo, GLuint* texture, int w, int h,
                                            GLenum internalFormat, GLenum format, GLenum type,
                                            bool depth) {
    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GLint(internalFormat), w, h, 0, format, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *texture, 0);
    if (depth) {
        GLuint depthRb = 0;
        glGenRenderbuffers(1, &depthRb);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRb);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 0, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRb);
    }
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return status == GL_FRAMEBUFFER_COMPLETE;
}

bool GlRenderer::createTargets(std::string* error) {
    const int w = surfaceWidth_ > 0 ? surfaceWidth_ : 1080;
    const int h = surfaceHeight_ > 0 ? surfaceHeight_ : 1920;
    GLint maxSamples = 1;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    const int samples = std::max(1, std::min(options_.msaaSamples, int(maxSamples)));
    options_.msaaSamples = samples;

    /* Resolved HDR image, sampled by bloom and composite. */
    if (!makeFramebufferWithTexture(&hdrFbo_, &hdrTexture_, w, h, hdrInternalFormat_, GL_RGBA,
                                   hdrInternalFormat_ == GL_RGBA16F ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE,
                                   false)) {
        *error = "HDR framebuffer incomplete (float colour buffers unavailable)";
        return false;
    }

    /* Multisampled renderbuffers the forward pass draws into; blitted into hdrFbo_ afterwards. */
    glGenFramebuffers(1, &msaaFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo_);
    glGenRenderbuffers(1, &msaaColorRb_);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaColorRb_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, hdrInternalFormat_, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaaColorRb_);
    glGenRenderbuffers(1, &msaaDepthRb_);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaDepthRb_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaaDepthRb_);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        /* The device refused the requested sample count; drop both attachments to 1x and try once
           more. The old fallback accidentally allocated the depth renderbuffer with the HDR colour
           format, guaranteeing an incomplete FBO exactly when auto quality changed tier. */
        glBindRenderbuffer(GL_RENDERBUFFER, msaaDepthRb_);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 1, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaaDepthRb_);
        glDeleteRenderbuffers(1, &msaaColorRb_);
        glGenRenderbuffers(1, &msaaColorRb_);
        glBindRenderbuffer(GL_RENDERBUFFER, msaaColorRb_);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 1, hdrInternalFormat_, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaaColorRb_);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            *error = "MSAA framebuffer incomplete even at 1x";
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }
        options_.msaaSamples = 1;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /* Shadow map: a depth texture with hardware comparison, so the shader's 5 taps each get a
       2x2 filtered comparison for free. */
    const int shadowSize = options_.enableShadows ? options_.shadowMapSize : 512;
    glGenTextures(1, &shadowTexture_);
    glBindTexture(GL_TEXTURE_2D, shadowTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowSize, shadowSize, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glGenFramebuffers(1, &shadowFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTexture_, 0);
    glDrawBuffers(0, nullptr);                  /* depth-only: no colour attachment at all */
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        *error = "shadow framebuffer incomplete";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /* Bloom chain. */
    bloomLevels_ = options_.enableBloom ? std::max(1, std::min(5, options_.bloomLevels)) : 0;
    int level = 0;
    for (int i = 0; i < bloomLevels_; ++i) {
        const int lw = std::max(1, w >> (i + 1));
        const int lh = std::max(1, h >> (i + 1));
        if (lw < 8 || lh < 8) break;
        if (!makeFramebufferWithTexture(&bloomFbo_[level], &bloomTexture_[level], lw, lh,
                                        hdrInternalFormat_, GL_RGBA,
                                        hdrInternalFormat_ == GL_RGBA16F ? GL_HALF_FLOAT
                                                                         : GL_UNSIGNED_BYTE,
                                        false)) {
            *error = "bloom framebuffer incomplete";
            return false;
        }
        bloomWidth_[level] = lw;
        bloomHeight_[level] = lh;
        ++level;
    }
    bloomLevels_ = level;
    stats_.bloomLevels = bloomLevels_;
    stats_.msaaSamples = options_.msaaSamples;
    stats_.shadowMapSize = options_.enableShadows ? options_.shadowMapSize : 0;
    return true;
}

/* ============================================================================================
 *  Setup
 * ============================================================================================ */
bool GlRenderer::init(ANativeWindow* window, const RendererOptions& options, std::string* error) {
    options_ = options;
    if (!createContext(window, error)) return false;
    if (!createPrograms(error)) return false;

    /* Buffer objects must exist before glVertexAttribPointer captures them in a VAO. */
    glGenBuffers(1, &vertexVbo_);
    glGenBuffers(1, &groundVbo_);
    glGenBuffers(1, &lineVbo_);
    if (!vertexVbo_ || !groundVbo_ || !lineVbo_) {
        *error = "failed to allocate GLES vertex buffer names";
        return false;
    }

    glGenVertexArrays(1, &avatarVao_);
    glGenVertexArrays(1, &groundVao_);
    glGenVertexArrays(1, &emptyVao_);
    glGenVertexArrays(1, &lineVao_);

    /* Avatar VAO: exact canonical af_vertex layout. The forward shader consumes locations 0..4.
       JOINTS/WEIGHTS at byte offsets 64/72 are CPU-side data today; no shader inputs 5/6 exist. */
    static_assert(sizeof(af_vertex) == 88, "GLES vertex stride must remain 88 bytes");
    static_assert(offsetof(af_vertex, pos) == 0 && offsetof(af_vertex, normal) == 12 &&
                  offsetof(af_vertex, tangent) == 24 && offsetof(af_vertex, uv) == 40 &&
                  offsetof(af_vertex, color) == 48 && offsetof(af_vertex, joints) == 64 &&
                  offsetof(af_vertex, weights) == 72, "af_vertex field offsets changed");
    glBindVertexArray(avatarVao_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(af_vertex),
                          reinterpret_cast<const void*>(offsetof(af_vertex, pos)));
    logVertexAttribute(0, 3, offsetof(af_vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(af_vertex),
                          reinterpret_cast<const void*>(offsetof(af_vertex, normal)));
    logVertexAttribute(1, 3, offsetof(af_vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(af_vertex),
                          reinterpret_cast<const void*>(offsetof(af_vertex, tangent)));
    logVertexAttribute(2, 4, offsetof(af_vertex, tangent));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(af_vertex),
                          reinterpret_cast<const void*>(offsetof(af_vertex, uv)));
    logVertexAttribute(3, 2, offsetof(af_vertex, uv));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(af_vertex),
                          reinterpret_cast<const void*>(offsetof(af_vertex, color)));
    logVertexAttribute(4, 4, offsetof(af_vertex, color));
    const char* attributeNames[5] = {"aPos", "aNormal", "aTangent", "aUv", "aColor"};
    for (GLint i = 0; i < 5; ++i) {
        const GLint shaderLocation = glGetAttribLocation(forwardProgram_, attributeNames[i]);
        AF_GL_LOGI("Shader input %s -> location %d (expected %d)", attributeNames[i], shaderLocation, i);
        if (shaderLocation != i) AF_GL_LOGE("Shader input location mismatch for %s", attributeNames[i]);
    }
    AF_GL_LOGI("af_vertex layout: size=%zu pos=0 normal=12 tangent=24 uv=40 color=48 joints=64 weights=72; locations 5/6 intentionally unused (CPU skinning path)",
               sizeof(af_vertex));
    const GLenum vaoSetupError = glGetError();
    if (vaoSetupError != GL_NO_ERROR) AF_GL_LOGE("VAO setup GL error=0x%x", vaoSetupError);
    glBindVertexArray(0);

    /* Ground VAO: positions only. */
    glBindVertexArray(groundVao_);
    glBindBuffer(GL_ARRAY_BUFFER, groundVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, nullptr);
    glBindVertexArray(0);

    /* Skeleton VAO: position + colour. */
    glBindVertexArray(lineVao_);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 7, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 7,
                          reinterpret_cast<const void*>(sizeof(float) * 3));
    glBindVertexArray(0);

    const float quad[12] = {-1.0f, 0.0f, -1.0f, 1.0f, 0.0f, -1.0f,
                            -1.0f, 0.0f,  1.0f, 1.0f, 0.0f,  1.0f};
    glBindBuffer(GL_ARRAY_BUFFER, groundVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    lineCapacity_ = GLint64(4096 * 28);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
    glBufferData(GL_ARRAY_BUFFER, lineCapacity_, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &cameraUbo_);
    glBindBuffer(GL_UNIFORM_BUFFER, cameraUbo_);
    glBufferData(GL_UNIFORM_BUFFER, GLsizeiptr(uboAlignment_) * 2, nullptr, GL_DYNAMIC_DRAW);
    glGenBuffers(1, &postUbo_);
    glBindBuffer(GL_UNIFORM_BUFFER, postUbo_);
    glBufferData(GL_UNIFORM_BUFFER, GLsizeiptr(uboAlignment_) * 16, nullptr, GL_DYNAMIC_DRAW);
    glGenBuffers(1, &materialUbo_);
    glBindBuffer(GL_UNIFORM_BUFFER, materialUbo_);
    glBufferData(GL_UNIFORM_BUFFER, GLsizeiptr(uboAlignment_) * 32, nullptr, GL_DYNAMIC_DRAW);
    materialCapacity_ = GLint64(uboAlignment_) * 32;
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    /* Atlas placeholders: 1x1 white, so the sampler bindings are always valid (the shaders gate
       every lookup on the material's has_*_tex flags). */
    const GLubyte white[4] = {255, 255, 255, 255};
    for (int i = 0; i < AF_TEX_COUNT; ++i) {
        glGenTextures(1, &atlasTexture_[i]);
        glBindTexture(GL_TEXTURE_2D, atlasTexture_[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if (!createTargets(error)) return false;
    activeTargetOptions_ = options_;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    if (engine_) {
        uploadAtlas(error);
        rebuildMaterialBlocks();
    }
    return true;
}

/* ============================================================================================
 *  Geometry and atlas
 * ============================================================================================ */
void GlRenderer::ensureGeometryBuffers() {
    if (!engine_) {
        geometryValid_ = false;
        vertexCount_ = indexCount_ = 0;
        return;
    }
    const int32_t vcount = af_engine_vertex_count(engine_);
    const int32_t icount = af_engine_index_count(engine_);
    const af_vertex* verts = af_engine_vertices(engine_);
    const uint32_t* indices = af_engine_indices(engine_);
    const int32_t submeshCount = af_engine_submesh_count(engine_);
    const af_submesh* submeshes = af_engine_submeshes(engine_);
    auto rejectGeometry = [&](const char* reason) {
        geometryValid_ = false;
        vertexCount_ = vcount;
        indexCount_ = icount;
        meshDirty_ = false;
        std::fprintf(stderr, "[AvatarForge] GLES geometry rejected: %s\n", reason);
    };
    if (!meshDirty_ && geometryValid_ && vertexCount_ == vcount && indexCount_ == icount) return;
    if (vcount <= 0 || icount <= 0 || !verts || !indices) {
        rejectGeometry("missing vertices or triangle indices");
        return;
    }
    if (submeshCount < 0 || (submeshCount > 0 && !submeshes)) {
        rejectGeometry("invalid submesh table");
        return;
    }
    if (GLint64(vcount) > std::numeric_limits<GLsizeiptr>::max() / VertexLayout::stride ||
        GLint64(icount) > std::numeric_limits<GLsizeiptr>::max() / GLint64(sizeof(uint32_t))) {
        rejectGeometry("GPU buffer byte size overflows GLsizeiptr");
        return;
    }

    /* Validate CPU-side indices and every draw range before any GPU upload or draw. */
    for (int32_t i = 0; i < icount; ++i) {
        if (indices[i] >= static_cast<uint32_t>(vcount)) {
            rejectGeometry("index references a vertex outside the vertex buffer");
            return;
        }
    }
    for (int32_t i = 0; i < submeshCount; ++i) {
        const af_submesh& sm = submeshes[i];
        if (sm.index_offset < 0 || sm.index_count < 0 ||
            sm.index_offset > icount || sm.index_count > icount - sm.index_offset) {
            rejectGeometry("submesh index range exceeds the index buffer");
            return;
        }
        if (sm.vertex_offset < 0 || sm.vertex_count < 0 ||
            sm.vertex_offset > vcount || sm.vertex_count > vcount - sm.vertex_offset) {
            rejectGeometry("submesh vertex range exceeds the vertex buffer");
            return;
        }
        if (sm.index_count > 0 && sm.vertex_count == 0) {
            rejectGeometry("non-empty submesh has an empty vertex range");
            return;
        }
        const uint32_t firstVertex = static_cast<uint32_t>(sm.vertex_offset);
        const uint32_t vertexEnd = firstVertex + static_cast<uint32_t>(sm.vertex_count);
        for (int32_t j = 0; j < sm.index_count; ++j) {
            const uint32_t index = indices[sm.index_offset + j];
            if (index < firstVertex || index >= vertexEnd) {
                rejectGeometry("submesh index references a vertex outside its declared vertex range");
                return;
            }
        }
        if ((sm.index_count % 3) != 0) {
            rejectGeometry("triangle submesh index count is not divisible by three");
            return;
        }
    }
    geometryValid_ = true;

    const GLint64 vbytes = GLint64(vcount) * VertexLayout::stride;
    glBindBuffer(GL_ARRAY_BUFFER, vertexVbo_);
    if (vbytes > vertexCapacity_) {
        const GLint64 capacity = vbytes + vbytes / 4;
        for (int drain = 0; drain < 16 && glGetError() != GL_NO_ERROR; ++drain) {}
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(capacity), nullptr, GL_DYNAMIC_DRAW);
        const GLenum allocError = glGetError();
        if (allocError != GL_NO_ERROR) {
            geometryValid_ = false;
            meshDirty_ = true;
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            AF_GL_LOGE("Vertex buffer allocation failed: error=0x%x bytes=%lld",
                       static_cast<unsigned int>(allocError), static_cast<long long>(capacity));
            return;
        }
        vertexCapacity_ = capacity;
        vertexCount_ = 0;   /* force a full upload below */
    }
    if (meshDirty_ || vertexCount_ != vcount) {
        if (!vertexDumpLogged_) {
            AF_GL_LOGI("GPU upload: vertices=%d bytes=%lld stride=%zu expectedBytes=%lld",
                       vcount, static_cast<long long>(vbytes), sizeof(af_vertex),
                       static_cast<long long>(GLint64(vcount) * 88));
            const int dumpCount = vcount < 5 ? vcount : 5;
            for (int i = 0; i < dumpCount; ++i) {
                const af_vertex& v = verts[i];
                AF_GL_LOGI("GPU Vertex %d: POS(%.7g,%.7g,%.7g) | NORM(%.7g,%.7g,%.7g) | UV(%.7g,%.7g) | COLOR(%.5g,%.5g,%.5g,%.5g)",
                           i, v.pos[0], v.pos[1], v.pos[2], v.normal[0], v.normal[1], v.normal[2],
                           v.uv[0], v.uv[1], v.color[0], v.color[1], v.color[2], v.color[3]);
            }
            vertexDumpLogged_ = true;
        }
        for (int drain = 0; drain < 16 && glGetError() != GL_NO_ERROR; ++drain) {}
        glBufferSubData(GL_ARRAY_BUFFER, 0, GLsizeiptr(vbytes), verts);
        const GLenum uploadError = glGetError();
        if (uploadError != GL_NO_ERROR) {
            geometryValid_ = false;
            meshDirty_ = true;
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            AF_GL_LOGE("Vertex upload failed: error=0x%x bytes=%lld sourceVerts=%d",
                       static_cast<unsigned int>(uploadError), static_cast<long long>(vbytes), vcount);
            return;
        }
        vertexCount_ = vcount;
    }

    if (icount > 0 && indices) {
        const GLint64 ibytes = GLint64(icount) * sizeof(uint32_t);
        if (indexIbo_ == 0) glGenBuffers(1, &indexIbo_);
        if (indexIbo_ == 0) {
            geometryValid_ = false;
            meshDirty_ = true;
            AF_GL_LOGE("Index buffer allocation failed: glGenBuffers returned 0");
            return;
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexIbo_);
        if (ibytes > indexCapacity_) {
            const GLint64 capacity = ibytes + ibytes / 4;
            for (int drain = 0; drain < 16 && glGetError() != GL_NO_ERROR; ++drain) {}
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(capacity), nullptr, GL_STATIC_DRAW);
            const GLenum allocError = glGetError();
            if (allocError != GL_NO_ERROR) {
                geometryValid_ = false;
                meshDirty_ = true;
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                AF_GL_LOGE("Index buffer allocation failed: error=0x%x bytes=%lld",
                           static_cast<unsigned int>(allocError), static_cast<long long>(capacity));
                return;
            }
            indexCapacity_ = capacity;
            indexCount_ = 0;
        }
        if (meshDirty_ || indexCount_ != icount) {
            for (int drain = 0; drain < 16 && glGetError() != GL_NO_ERROR; ++drain) {}
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, GLsizeiptr(ibytes), indices);
            const GLenum uploadError = glGetError();
            if (uploadError != GL_NO_ERROR) {
                geometryValid_ = false;
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                AF_GL_LOGE("Index upload failed: error=0x%x bytes=%lld sourceIndices=%d",
                           static_cast<unsigned int>(uploadError), static_cast<long long>(ibytes), icount);
                return;
            }
            indexCount_ = icount;
        }
    }
    vertexCount_ = vcount;
    indexCount_ = icount;
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    meshDirty_ = false;
}

void GlRenderer::uploadAtlas(std::string* error) {
    if (!engine_) return;
    for (int slot = 0; slot < AF_TEX_COUNT; ++slot) {
        af_texture tex{};
        if (af_engine_texture(engine_, slot, &tex) != AF_OK) continue;
        if (!tex.data || tex.width <= 0 || tex.height <= 0) continue;

        const GLenum internal = (tex.channels == 1) ? GL_R8
                               : (slot == AF_TEX_BASE_COLOR) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        const GLenum format = (tex.channels == 1) ? GL_RED : GL_RGBA;
        glBindTexture(GL_TEXTURE_2D, atlasTexture_[slot]);
        glTexImage2D(GL_TEXTURE_2D, 0, GLint(internal), tex.width, tex.height, 0, format,
                     GL_UNSIGNED_BYTE, tex.data);
        glGenerateMipmap(GL_TEXTURE_2D);   /* GLES has this built in; the Vulkan path blits */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        /* The generated chain must not collapse to magenta when a driver limits mip depth. */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 12);

        const char* ext = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if (ext && std::strstr(ext, "GL_EXT_texture_filter_anisotropic")) {
            GLfloat maxAniso = 1.0f;
            glGetFloatv(0x84FF /* GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT */, &maxAniso);
            glTexParameterf(GL_TEXTURE_2D, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY_EXT */,
                            std::min(4.0f, maxAniso));
        }
    }
    atlasUploaded_ = true;
    (void)error;
}

void GlRenderer::rebuildMaterialBlocks() {
    if (!engine_) return;
    const int submeshCount = af_engine_submesh_count(engine_);
    const af_submesh* submeshes = af_engine_submeshes(engine_);
    const af_material* materials = af_engine_materials(engine_);
    const int materialCount = af_engine_material_count(engine_);
    if (submeshCount <= 0 || !submeshes) { materialCount_ = 0; return; }

    materialStride_ = uboAlignment_ > sizeof(MaterialUbo) ? uboAlignment_
                                                          : (sizeof(MaterialUbo) + 15u) / 16u * 16u;
    const GLint64 needed = GLint64(submeshCount + 1) * materialStride_;
    glBindBuffer(GL_UNIFORM_BUFFER, materialUbo_);
    if (needed > materialCapacity_) {
        glBufferData(GL_UNIFORM_BUFFER, GLsizeiptr(needed * 2), nullptr, GL_DYNAMIC_DRAW);
        materialCapacity_ = needed * 2;
    }

    for (int i = 0; i < submeshCount; ++i) {
        const af_submesh& sm = submeshes[i];
        const af_material& m = (materials && sm.material_index >= 0 && sm.material_index < materialCount)
                                   ? materials[sm.material_index] : af_material{};
        MaterialUbo block{};
        block.baseColorFactor[0] = m.base_color_factor[0];
        block.baseColorFactor[1] = m.base_color_factor[1];
        block.baseColorFactor[2] = m.base_color_factor[2];
        block.baseColorFactor[3] = m.base_color_factor[3];
        block.pbr[0] = m.metallic;
        block.pbr[1] = m.roughness;
        block.pbr[2] = m.ior;
        block.pbr[3] = m.sheen;
        block.pbr2[0] = m.subsurface;
        block.pbr2[1] = m.anisotropic;
        block.pbr2[2] = float(materialKindForRegion(sm.region, m.metallic));
        block.pbr2[3] = m.has_normal_tex ? 1.0f : 0.0f;
        block.texFlags[0] = m.has_base_color_tex ? 1.0f : 0.0f;
        block.texFlags[1] = m.has_mr_tex ? 1.0f : 0.0f;
        block.texFlags[2] = m.has_ao_tex ? 1.0f : 0.0f;
        block.texFlags[3] = atlasUploaded_ ? 1.0f : 0.0f;
        block.region[0] = float(sm.region);
        block.region[1] = (highlightRegion_ >= 0 && sm.region == highlightRegion_) ? 1.0f : 0.0f;
        /* Eye regions additionally carry the uv extent of their atlas island plus a flag: the eye
           shader needs it to turn a refraction offset in metres into an atlas offset, so the iris
           parallax is right at whatever texture resolution the projector chose. */
        block.region[2] = irisUvExtent(engine_, sm);
        block.region[3] = (block.pbr2[2] == float(AF_KIND_EYE)) ? 1.0f : 0.0f;
        glBufferSubData(GL_UNIFORM_BUFFER, GLintptr(i) * materialStride_, sizeof(block), &block);
    }
    MaterialUbo ground{};
    ground.baseColorFactor[0] = ground.baseColorFactor[1] = ground.baseColorFactor[2] = 0.32f;
    ground.baseColorFactor[3] = 1.0f;
    ground.pbr[1] = 0.75f;
    ground.pbr[2] = 1.5f;
    ground.pbr2[2] = float(AF_KIND_GENERIC);
    ground.region[0] = -1.0f;
    glBufferSubData(GL_UNIFORM_BUFFER, GLintptr(submeshCount) * materialStride_, sizeof(ground), &ground);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    materialCount_ = submeshCount;
}

void GlRenderer::buildSkeletonLines() {
    lineVertexCount_ = 0;
    if (!engine_ || !skeletonVisible_) return;
    const int J = af_engine_joint_count(engine_);
    if (J <= 0) return;
    std::vector<float> matrices(size_t(J) * 16);
    if (af_engine_joint_matrices(engine_, matrices.data(), J) != AF_OK) return;

    std::vector<float> verts;
    verts.reserve(size_t(J) * 2 * 7);
    for (int j = 0; j < J; ++j) {
        af_joint_info info{};
        if (af_engine_joint_info(engine_, j, &info) != AF_OK) continue;
        if (info.parent < 0 || info.parent >= J) continue;
        const float* a = &matrices[size_t(j) * 16];
        const float* b = &matrices[size_t(info.parent) * 16];
        float rgb[3] = {0.35f, 0.72f, 1.00f};
        if (info.flags & AF_JOINT_FACE) { rgb[0] = 1.00f; rgb[1] = 0.62f; rgb[2] = 0.18f; }
        if (info.flags & AF_JOINT_FINGER) { rgb[0] = 0.30f; rgb[1] = 1.00f; rgb[2] = 0.78f; }
        if (info.flags & AF_JOINT_TWIST) { rgb[0] = 0.78f; rgb[1] = 0.42f; rgb[2] = 1.00f; }
        verts.insert(verts.end(), {a[12], a[13], a[14], rgb[0], rgb[1], rgb[2], 1.0f});
        verts.insert(verts.end(), {b[12], b[13], b[14], rgb[0], rgb[1], rgb[2], 0.65f});
    }
    lineVertexCount_ = uint32_t(verts.size() / 7);
    if (lineVertexCount_ == 0) return;
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
    const GLsizeiptr bytes = GLsizeiptr(verts.size() * sizeof(float));
    if (GLint64(bytes) > lineCapacity_) {
        glBufferData(GL_ARRAY_BUFFER, bytes, verts.data(), GL_DYNAMIC_DRAW);
        lineCapacity_ = bytes;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, verts.data());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* The VAO captured the buffer binding at creation time (it was created before the first
       upload), which is fine because glVertexAttribPointer stores the binding, not the data. */
}

/* ============================================================================================
 *  Per-frame
 * ============================================================================================ */
void GlRenderer::updateScene(float deltaSeconds) {
    if (std::isfinite(deltaSeconds) && deltaSeconds > 0.0f) {
        cameraLogAccumulatorSeconds_ += deltaSeconds;
    }
    env_.sunAzimuthDeg = options_.sunAzimuthDeg;
    env_.sunElevationDeg = options_.sunElevationDeg;
    env_.sunIntensity = options_.sunIntensity;
    env_.turbidity = options_.turbidity;
    env_.groundAlbedo = options_.groundAlbedo;
    env_.intensity = options_.environmentIntensity;
    projectToSh(env_, &sh_, 1024);

    modelBounds(engine_, scene_.modelMin, scene_.modelMax, &scene_.hasModel);
    /* depthZeroToOne = false: GL clip space, which is also why the ES dialect of the shaders has
       AF_CLIP_Y = +1 and rescales the shadow lookup's z. */
    buildCameraUbo(&scene_, camera_, options_, env_, sh_, surfaceWidth_, surfaceHeight_, false);

    /* Exact camera/frustum dump once per elapsed second, using the matrix uploaded this frame. */
    if (cameraLogAccumulatorSeconds_ >= 1.0f) {
        cameraLogAccumulatorSeconds_ = std::fmod(cameraLogAccumulatorSeconds_, 1.0f);
        AF_GL_LOGW(
            "CAMERA_DIAG target=(%.9g,%.9g,%.9g) eye=(%.9g,%.9g,%.9g) "
            "distance=%.9g fov=%.9g zNear=%.9g zFar=%.9g viewport=%dx%d",
            camera_.targetX, camera_.targetY, camera_.targetZ,
            scene_.eye.x, scene_.eye.y, scene_.eye.z,
            camera_.distance, camera_.fovDeg, scene_.nearPlane, scene_.farPlane,
            surfaceWidth_, surfaceHeight_);
        const float* vp = scene_.camera.viewProj;
        for (int row = 0; row < 4; ++row) {
            AF_GL_LOGW("CAMERA_DIAG viewProj.row%d=[%.9g, %.9g, %.9g, %.9g]",
                       row, vp[row], vp[4 + row], vp[8 + row], vp[12 + row]);
        }
        const float* view = scene_.camera.view;
        for (int row = 0; row < 4; ++row) {
            AF_GL_LOGW("CAMERA_DIAG view.row%d=[%.9g, %.9g, %.9g, %.9g]",
                       row, view[row], view[4 + row], view[8 + row], view[12 + row]);
        }
    }

    if (!cameraDumpLogged_) {
        bool matrixFinite = true;
        for (int i = 0; i < 16; ++i) matrixFinite = matrixFinite && std::isfinite(scene_.camera.viewProj[i]);
        AF_GL_LOGI("MVP/viewProj finite=%d target=(%.7g,%.7g,%.7g) distance=%.7g fov=%.7g viewport=%dx%d",
                   matrixFinite ? 1 : 0, camera_.targetX, camera_.targetY, camera_.targetZ,
                   camera_.distance, camera_.fovDeg, surfaceWidth_, surfaceHeight_);
        if (!matrixFinite) AF_GL_LOGE("Camera view-projection matrix contains NaN/Infinity");
        const af_vertex* cameraVerts = engine_ ? af_engine_vertices(engine_) : nullptr;
        const int32_t cameraVertexCount = engine_ ? af_engine_vertex_count(engine_) : 0;
        const int dumpCount = cameraVertexCount < 5 ? cameraVertexCount : 5;
        for (int i = 0; cameraVerts && i < dumpCount; ++i) {
            const float* m = scene_.camera.viewProj; // column-major, column vectors
            const af_vertex& v = cameraVerts[i];
            const float x = v.pos[0], y = v.pos[1], z = v.pos[2];
            const float cx = m[0]*x + m[4]*y + m[8]*z + m[12];
            const float cy = m[1]*x + m[5]*y + m[9]*z + m[13];
            const float cz = m[2]*x + m[6]*y + m[10]*z + m[14];
            const float cw = m[3]*x + m[7]*y + m[11]*z + m[15];
            AF_GL_LOGI("MVP clip vertex %d: (%.7g,%.7g,%.7g,%.7g) finite=%d nonzeroW=%d",
                       i, cx, cy, cz, cw,
                       (std::isfinite(cx) && std::isfinite(cy) && std::isfinite(cz) && std::isfinite(cw)) ? 1 : 0,
                       (std::isfinite(cw) && std::fabs(cw) > 1.0e-7f) ? 1 : 0);
        }
        cameraDumpLogged_ = true;
    }
    scene_.camera.envFlags[3] = flatShading_ ? 1.0f : 0.0f;
    scene_.camera.uiParams[0] = float(highlightRegion_);
    scene_.camera.uiParams[1] = highlightRegion_ >= 0 ? 1.0f : 0.0f;
    scene_.camera.uiParams[2] = float(options_.shadowSoftness) /
                                float(options_.shadowMapSize ? options_.shadowMapSize : 2048);
    scene_.camera.uiParams[3] = 1.0f / float(surfaceHeight_ ? surfaceHeight_ : 1);

    glBindBuffer(GL_UNIFORM_BUFFER, cameraUbo_);
    const int frame = int(stats_.frames % 2);
    glBufferSubData(GL_UNIFORM_BUFFER, GLintptr(frame) * uboAlignment_, sizeof(CameraUbo), &scene_.camera);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    /* Post slices, laid out exactly like the Vulkan path so the two cannot drift. */
    if (bloomLevels_ > 0) {
        PostUbo p{};
        const float hdrThreshold = (hdrInternalFormat_ == GL_RGBA16F) ? 1.0f : 0.62f;
        for (int i = 0; i < bloomLevels_; ++i) {
            p.params[0] = (i == 0) ? hdrThreshold : 0.0f;
            p.params[1] = 0.5f;
            p.params[2] = p.params[3] = 0.0f;
            const int sw = (i == 0) ? surfaceWidth_ : bloomWidth_[i - 1];
            const int sh = (i == 0) ? surfaceHeight_ : bloomHeight_[i - 1];
            p.texel[0] = 1.0f / float(sw ? sw : 1);
            p.texel[1] = 1.0f / float(sh ? sh : 1);
            p.texel[2] = p.texel[3] = 0.0f;
            glBindBuffer(GL_UNIFORM_BUFFER, postUbo_);
            glBufferSubData(GL_UNIFORM_BUFFER, GLintptr(i) * uboAlignment_, sizeof(PostUbo), &p);
        }
        for (int j = 0; j + 1 < bloomLevels_; ++j) {
            p.params[0] = p.params[1] = p.params[2] = p.params[3] = 0.0f;
            p.texel[0] = 1.0f / float(bloomWidth_[j] ? bloomWidth_[j] : 1);
            p.texel[1] = 1.0f / float(bloomHeight_[j] ? bloomHeight_[j] : 1);
            p.texel[2] = p.texel[3] = 0.0f;
            glBindBuffer(GL_UNIFORM_BUFFER, postUbo_);
            glBufferSubData(GL_UNIFORM_BUFFER, GLintptr(bloomLevels_ + j) * uboAlignment_,
                            sizeof(PostUbo), &p);
        }
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
    PostUbo c{};
    c.params[2] = options_.enableBloom ? 0.055f : 0.0f;
    c.params[3] = options_.exposure;
    c.texel[0] = 1.0f / float(surfaceWidth_ ? surfaceWidth_ : 1);
    c.texel[1] = 1.0f / float(surfaceHeight_ ? surfaceHeight_ : 1);
    glBindBuffer(GL_UNIFORM_BUFFER, postUbo_);
    glBufferSubData(GL_UNIFORM_BUFFER, GLintptr(2 * bloomLevels_) * uboAlignment_, sizeof(PostUbo), &c);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, cameraUbo_);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, postUbo_);

    rebuildMaterialBlocks();
    buildSkeletonLines();
}

void GlRenderer::updateAnimation(float deltaSeconds) {
    if (!engine_ || clip_ < 0) return;
    const float duration = af_engine_clip_duration(engine_, clip_);
    if (duration <= 0.0f) return;
    clipTime_ += deltaSeconds * clipSpeed_;
    if (clipLoop_) {
        clipTime_ = std::fmod(clipTime_, duration);
        if (clipTime_ < 0.0f) clipTime_ += duration;
    } else if (clipTime_ > duration) {
        clipTime_ = duration;
    }
    if (af_engine_apply_clip(engine_, clip_, clipTime_) == AF_OK) meshDirty_ = true;
}

void GlRenderer::setClip(int clip, float speed, bool loop) {
    clip_ = clip;
    clipSpeed_ = speed;
    clipLoop_ = loop;
    clipTime_ = 0.0f;
    if (engine_ && clip_ >= 0 && af_engine_apply_clip(engine_, clip_, 0.0f) == AF_OK) meshDirty_ = true;
}

/* ============================================================================================
 *  Draw
 * ============================================================================================ */
void GlRenderer::drawShadowPass() {
    if (!options_.enableShadows || !engine_ || !geometryValid_ || indexCount_ == 0) return;
    const int size = options_.shadowMapSize;
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glViewport(0, 0, size, size);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);

    /* Slope-scaled bias is not part of ES; the equivalent constant is applied here and the
       normal-offset in the shader handles the rest. glPolygonOffset does exactly this. */
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.25f, 1.75f);
    glUseProgram(shadowProgram_);
    glBindVertexArray(avatarVao_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexVbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexIbo_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glDisable(GL_POLYGON_OFFSET_FILL);
    ++drawCalls_;
}

void GlRenderer::drawForwardPass() {
    const int frame = int(stats_.frames % 2);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo_);
    glViewport(0, 0, surfaceWidth_, surfaceHeight_);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glClearColor(0.055f, 0.075f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindBufferRange(GL_UNIFORM_BUFFER, 0, cameraUbo_, GLintptr(frame) * uboAlignment_, sizeof(CameraUbo));

    /* ---- studio ground ---- */
    if (!kDebugMagentaAvatar && options_.groundPlane && engine_ && vertexCount_ > 0) {
        glUseProgram(groundProgram_);
        glBindVertexArray(groundVao_);
        glBindBuffer(GL_ARRAY_BUFFER, groundVbo_);
        glBindBufferRange(GL_UNIFORM_BUFFER, 1, materialUbo_,
                          GLintptr(materialCount_) * materialStride_, sizeof(MaterialUbo));
        glBindTexture(GL_TEXTURE_2D, shadowTexture_);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, shadowTexture_);
        glActiveTexture(GL_TEXTURE0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        ++drawCalls_;
    }

    /* ---- the avatar: one draw per submesh so each gets its own material block ---- */
    const bool avatarDrawable = engine_ && geometryValid_ && vertexCount_ > 0 && indexCount_ > 0;
    if (!avatarDrawable && stats_.frames % 120 == 0) {
        AF_GL_LOGW("Avatar draw skipped: engine=%d geometryValid=%d uploadedVerts=%d uploadedIndices=%d "
                   "engineVerts=%d engineIndices=%d sceneHasModel=%d",
                   engine_ ? 1 : 0, geometryValid_ ? 1 : 0, vertexCount_, indexCount_,
                   engine_ ? af_engine_vertex_count(engine_) : 0,
                   engine_ ? af_engine_index_count(engine_) : 0,
                   scene_.hasModel ? 1 : 0);
    }
    if (avatarDrawable) {
        const GLuint avatarProgram = kDebugMagentaAvatar ? debugMagentaProgram_ : forwardProgram_;
        if (!avatarProgram) {
            AF_GL_LOGE("Avatar draw aborted: selected shader program is zero");
            return;
        }
        glUseProgram(avatarProgram);
        const GLboolean cullingWasEnabled = glIsEnabled(GL_CULL_FACE);
        if (kDebugMagentaAvatar) {
            glDisable(GL_CULL_FACE);
            AF_GL_LOGW("DEBUG_MAGENTA active: avatar PBR, textures, material UBO and culling bypassed");
        }
        glBindVertexArray(avatarVao_);
        glBindBuffer(GL_ARRAY_BUFFER, vertexVbo_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexIbo_);

        if (!kDebugMagentaAvatar) {
            for (int slot = 0; slot < AF_TEX_COUNT; ++slot) {
                glActiveTexture(GL_TEXTURE0 + slot);
                glBindTexture(GL_TEXTURE_2D, atlasTexture_[slot]);
            }
            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, shadowTexture_);
            glActiveTexture(GL_TEXTURE0);
        }

        const af_submesh* submeshes = af_engine_submeshes(engine_);
        const int n = af_engine_submesh_count(engine_);
        for (int i = 0; i < n; ++i) {
            const af_submesh& sm = submeshes[i];
            /* Re-check immediately before converting the element offset to a byte pointer. */
            if (sm.index_offset < 0 || sm.index_count < 0 ||
                sm.index_offset > indexCount_ || sm.index_count > indexCount_ - sm.index_offset) {
                geometryValid_ = false;
                std::fprintf(stderr, "[AvatarForge] skipped invalid submesh draw range %d\n", i);
                continue;
            }
            if (!kDebugMagentaAvatar) {
                glBindBufferRange(GL_UNIFORM_BUFFER, 1, materialUbo_, GLintptr(i) * materialStride_,
                                  sizeof(MaterialUbo));
            }
            const size_t byteOffset = static_cast<size_t>(sm.index_offset) * sizeof(uint32_t);
            if (!drawDumpLogged_) {
                AF_GL_LOGI("DrawElements submesh=%d mode=TRIANGLES count=%d indexType=UNSIGNED_INT indexOffset=%d byteOffset=%zu vertexRange=[%d,%d)",
                           i, sm.index_count, sm.index_offset, byteOffset, sm.vertex_offset,
                           sm.vertex_offset + sm.vertex_count);
                drawDumpLogged_ = true;
            }
            GLenum preDrawError = GL_NO_ERROR;
            for (int drain = 0; drain < 16; ++drain) {
                const GLenum pending = glGetError();
                if (pending == GL_NO_ERROR) break;
                if (preDrawError == GL_NO_ERROR) preDrawError = pending;
            }
            if (preDrawError != GL_NO_ERROR) {
                if (preDrawError != lastDrawError_ || stats_.frames % 120 == 0) {
                    AF_GL_LOGE("GLES state error before avatar draw: error=0x%x submesh=%d frame=%llu",
                               static_cast<unsigned int>(preDrawError), i,
                               static_cast<unsigned long long>(stats_.frames));
                }
                lastDrawError_ = preDrawError;
                continue;
            }
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(sm.index_count), GL_UNSIGNED_INT,
                           reinterpret_cast<const void*>(byteOffset));
            const GLenum drawError = glGetError();
            if (drawError != GL_NO_ERROR) {
                if (drawError != lastDrawError_ || stats_.frames % 120 == 0) {
                    AF_GL_LOGE("glDrawElements failed: error=0x%x submesh=%d count=%d offset=%d vao=%d",
                               static_cast<unsigned int>(drawError), i, sm.index_count, sm.index_offset,
                               static_cast<int>(avatarVao_));
                }
                lastDrawError_ = drawError;
                continue;
            }
            lastDrawError_ = GL_NO_ERROR;
            ++drawCalls_;
            triangles_ += sm.index_count / 3;
        }
        if (kDebugMagentaAvatar && cullingWasEnabled) glEnable(GL_CULL_FACE);
    }

    /* ---- skeleton overlay ---- */
    if (skeletonVisible_ && lineVertexCount_ >= 2) {
        glUseProgram(lineProgram_);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
        glDrawArrays(GL_LINES, 0, GLsizei(lineVertexCount_));
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        ++drawCalls_;
    }

    /* ---- resolve the MSAA buffer into the HDR texture ---- */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hdrFbo_);
    glBlitFramebuffer(0, 0, surfaceWidth_, surfaceHeight_, 0, 0, surfaceWidth_, surfaceHeight_,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GlRenderer::drawBloom() {
    if (bloomLevels_ <= 0) return;
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glBindVertexArray(emptyVao_);

    for (int i = 0; i < bloomLevels_; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[i]);
        glViewport(0, 0, bloomWidth_[i], bloomHeight_[i]);
        glUseProgram(bloomDownProgram_);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, (i == 0) ? hdrTexture_ : bloomTexture_[i - 1]);
        glBindBufferRange(GL_UNIFORM_BUFFER, 2, postUbo_, GLintptr(i) * uboAlignment_, sizeof(PostUbo));
        glDrawArrays(GL_TRIANGLES, 0, 3);
        ++drawCalls_;
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (int i = 0; i + 1 < bloomLevels_; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[i]);
        glViewport(0, 0, bloomWidth_[i], bloomHeight_[i]);
        glUseProgram(bloomUpProgram_);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, bloomTexture_[i + 1]);
        glBindBufferRange(GL_UNIFORM_BUFFER, 2, postUbo_, GLintptr(bloomLevels_ + i) * uboAlignment_,
                          sizeof(PostUbo));
        glDrawArrays(GL_TRIANGLES, 0, 3);
        ++drawCalls_;
    }
    glDisable(GL_BLEND);
}

void GlRenderer::drawComposite() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, surfaceWidth_, surfaceHeight_);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glBindVertexArray(emptyVao_);
    glUseProgram(compositeProgram_);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, hdrTexture_);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, bloomLevels_ > 0 ? bloomTexture_[0] : hdrTexture_);
    glActiveTexture(GL_TEXTURE0);
    glBindBufferRange(GL_UNIFORM_BUFFER, 2, postUbo_, GLintptr(2 * bloomLevels_) * uboAlignment_,
                      sizeof(PostUbo));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    ++drawCalls_;
    /* The window's default framebuffer has no depth; restore the state the next frame expects. */
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void GlRenderer::renderFrame(float deltaSeconds) {
    stats_.framePresented = false;
    stats_.readbackValid = false;
    stats_.readbackNonBlackPixels = 0;
    stats_.readbackWidth = 0;
    stats_.readbackHeight = 0;
    stats_.glError = 0;
    stats_.eglError = 0;
    verificationPixels_.clear();
    if (contextLost_ || display_ == EGL_NO_DISPLAY) return;
    if (targetsDirty_) {
        targetsDirty_ = false;
        const RendererOptions rollback = activeTargetOptions_;
        destroyTargets();
        std::string error;
        if (createTargets(&error)) {
            activeTargetOptions_ = options_;
        } else {
            /* A quality transition is never allowed to turn a valid frame into an empty stage.
               Clean partial allocations and reconstruct the last known-good target set in the same
               frame. Only a lost EGL context may stop drawing. */
            destroyTargets();
            options_ = rollback;
            std::string rollbackError;
            if (!createTargets(&rollbackError)) {
                contextLost_ = true;
                return;
            }
            activeTargetOptions_ = options_;
        }
    }

    stats_.frames++;
    if (deltaSeconds > 0.0f && deltaSeconds < 1.0f) {
        const float alpha = 0.06f;
        stats_.frameMs = stats_.frameMs * (1.0f - alpha) + deltaSeconds * 1000.0f * alpha;
        stats_.fps = stats_.frameMs > 0.01f ? 1000.0f / stats_.frameMs : 0.0f;
    }
    drawCalls_ = 0;
    triangles_ = 0;

    /* A resize between frames walks the whole target set; the window surface itself is resized by
       the compositor and picked up through eglQuerySurface. */
    int w = 0, h = 0;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &w);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &h);
    if (w > 0 && h > 0 && (w != surfaceWidth_ || h != surfaceHeight_)) {
        if (!resize(w, h)) return;
    }

    /* Deform first: updateScene derives camera bounds and clipping planes from the live mesh. */
    updateAnimation(deltaSeconds);
    updateScene(deltaSeconds);
    ensureGeometryBuffers();

    stats_.nearPlane = scene_.nearPlane;
    stats_.farPlane = scene_.farPlane;
    stats_.minViewDepth = std::numeric_limits<float>::infinity();
    stats_.maxViewDepth = -std::numeric_limits<float>::infinity();
    int outsidePlane[6] = {0, 0, 0, 0, 0, 0};
    stats_.outsideCorners = 0;
    stats_.firstOutsideCorner = -1;
    stats_.firstOutsideViewX = stats_.firstOutsideViewY = stats_.firstOutsideViewZ = 0.0f;
    stats_.boundsAllCornersInDepthRange = scene_.hasModel;
    if (scene_.hasModel) {
        for (int corner = 0; corner < 8; ++corner) {
            const float x = (corner & 1) ? scene_.modelMax[0] : scene_.modelMin[0];
            const float y = (corner & 2) ? scene_.modelMax[1] : scene_.modelMin[1];
            const float z = (corner & 4) ? scene_.modelMax[2] : scene_.modelMin[2];
            const float* v = scene_.camera.view;
            const float vx = v[0]*x + v[4]*y + v[8]*z + v[12];
            const float vy = v[1]*x + v[5]*y + v[9]*z + v[13];
            const float vz = v[2]*x + v[6]*y + v[10]*z + v[14];
            const float depth = -vz;
            const bool depthInside = depth >= scene_.nearPlane && depth <= scene_.farPlane;
            stats_.boundsAllCornersInDepthRange = stats_.boundsAllCornersInDepthRange && depthInside;
            stats_.minViewDepth = std::min(stats_.minViewDepth, depth);
            stats_.maxViewDepth = std::max(stats_.maxViewDepth, depth);
            const float* m = scene_.camera.viewProj;
            const float cx = m[0]*x + m[4]*y + m[8]*z + m[12];
            const float cy = m[1]*x + m[5]*y + m[9]*z + m[13];
            const float cz = m[2]*x + m[6]*y + m[10]*z + m[14];
            const float cw = m[3]*x + m[7]*y + m[11]*z + m[15];
            if (cx < -cw) ++outsidePlane[0];
            if (cx >  cw) ++outsidePlane[1];
            if (cy < -cw) ++outsidePlane[2];
            if (cy >  cw) ++outsidePlane[3];
            if (cz < -cw) ++outsidePlane[4];
            if (cz >  cw) ++outsidePlane[5];
            const bool cornerOutside = !depthInside || cx < -cw || cx > cw || cy < -cw || cy > cw || cz < -cw || cz > cw;
            if (cornerOutside) {
                if (stats_.firstOutsideCorner < 0) {
                    stats_.firstOutsideCorner = corner;
                    stats_.firstOutsideViewX = vx;
                    stats_.firstOutsideViewY = vy;
                    stats_.firstOutsideViewZ = vz;
                }
                ++stats_.outsideCorners;
            }
        }
        bool rejectedByPlane = false;
        for (int count : outsidePlane) rejectedByPlane = rejectedByPlane || count == 8;
        const bool depthOverlap = stats_.maxViewDepth >= scene_.nearPlane &&
                                  stats_.minViewDepth <= scene_.farPlane;
        stats_.boundsInFrustum = !rejectedByPlane && depthOverlap &&
                                 std::isfinite(stats_.minViewDepth) &&
                                 std::isfinite(stats_.maxViewDepth);
    } else {
        stats_.boundsInFrustum = false;
        stats_.boundsAllCornersInDepthRange = false;
    }

    drawShadowPass();
    drawForwardPass();
    drawBloom();
    drawComposite();

    /* Deterministic test readback happens from the final default framebuffer, before presentation. */
    if (options_.verifyFramePixels) {
        glFinish();
        GLenum drawError = GL_NO_ERROR;
        for (GLenum err = glGetError(); err != GL_NO_ERROR; err = glGetError()) {
            if (drawError == GL_NO_ERROR) drawError = err;
        }
        stats_.glError = static_cast<int>(drawError);
        if (drawError == GL_NO_ERROR && surfaceWidth_ > 0 && surfaceHeight_ > 0) {
            const size_t pixelCount = static_cast<size_t>(surfaceWidth_) * static_cast<size_t>(surfaceHeight_);
            verificationPixels_.resize(pixelCount * 4u);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glReadPixels(0, 0, surfaceWidth_, surfaceHeight_, GL_RGBA, GL_UNSIGNED_BYTE,
                         verificationPixels_.data());
            const GLenum readError = glGetError();
            if (readError == GL_NO_ERROR) {
                /* Count geometry against the sampled clear colour, not against pure black.
                   The renderer's dark blue-grey clear colour is itself above RGB 10. */
                const size_t cornerIndices[4] = {
                    0u,
                    static_cast<size_t>(surfaceWidth_ - 1),
                    static_cast<size_t>(surfaceHeight_ - 1) * static_cast<size_t>(surfaceWidth_),
                    pixelCount - 1u
                };
                uint8_t cornerR[4], cornerG[4], cornerB[4];
                for (int corner = 0; corner < 4; ++corner) {
                    const uint8_t* px = &verificationPixels_[cornerIndices[corner] * 4u];
                    cornerR[corner] = px[0];
                    cornerG[corner] = px[1];
                    cornerB[corner] = px[2];
                }
                std::sort(cornerR, cornerR + 4);
                std::sort(cornerG, cornerG + 4);
                std::sort(cornerB, cornerB + 4);
                const int backgroundR = (static_cast<int>(cornerR[1]) + cornerR[2]) / 2;
                const int backgroundG = (static_cast<int>(cornerG[1]) + cornerG[2]) / 2;
                const int backgroundB = (static_cast<int>(cornerB[1]) + cornerB[2]) / 2;
                int visible = 0;
                for (size_t i = 0; i < pixelCount; ++i) {
                    const uint8_t* px = &verificationPixels_[i * 4u];
                    const int dr = px[0] > backgroundR ? px[0] - backgroundR : backgroundR - px[0];
                    const int dg = px[1] > backgroundG ? px[1] - backgroundG : backgroundG - px[1];
                    const int db = px[2] > backgroundB ? px[2] - backgroundB : backgroundB - px[2];
                    const int maxDelta = std::max(dr, std::max(dg, db));
                    if (dr + dg + db >= 48 && maxDelta >= 16) ++visible;
                }
                stats_.readbackValid = true;
                stats_.readbackNonBlackPixels = visible;
                stats_.readbackWidth = surfaceWidth_;
                stats_.readbackHeight = surfaceHeight_;
            } else {
                stats_.glError = static_cast<int>(readError);
                verificationPixels_.clear();
            }
        }
    }

    const EGLBoolean swapped = eglSwapBuffers(display_, surface_);
    if (swapped != EGL_TRUE) {
        stats_.eglError = static_cast<int>(eglGetError());
        if (stats_.eglError == EGL_CONTEXT_LOST || stats_.eglError == EGL_BAD_SURFACE) contextLost_ = true;
    } else {
        stats_.framePresented = stats_.glError == 0;
        if (stats_.framePresented) ++stats_.presentedFrames;
    }

    stats_.drawCalls = drawCalls_;
    stats_.triangles = triangles_;
    stats_.vertices = vertexCount_;
}

/* ============================================================================================
 *  Interface plumbing and teardown
 * ============================================================================================ */
void GlRenderer::setOptions(const RendererOptions& options) {
    const bool needsRebuild =
        options.msaaSamples != options_.msaaSamples ||
        options.enableShadows != options_.enableShadows ||
        (options.enableShadows && options.shadowMapSize != options_.shadowMapSize) ||
        options.enableBloom != options_.enableBloom ||
        (options.enableBloom && options.bloomLevels != options_.bloomLevels);
    options_ = options;
    if (needsRebuild && context_ != EGL_NO_CONTEXT) targetsDirty_ = true;
}

void GlRenderer::attachEngine(af_engine* engine) {
    engine_ = engine;
    meshDirty_ = true;
    atlasUploaded_ = false;
    vertexDumpLogged_ = false;
    cameraDumpLogged_ = false;
    drawDumpLogged_ = false;
    if (engine_ && context_ != EGL_NO_CONTEXT) {
        std::string error;
        uploadAtlas(&error);
        rebuildMaterialBlocks();
    }
}

void GlRenderer::invalidateMesh() { meshDirty_ = true; }

bool GlRenderer::resize(int width, int height) {
    if (context_ == EGL_NO_CONTEXT || width <= 0 || height <= 0) return false;
    surfaceWidth_ = width;
    surfaceHeight_ = height;
    destroyTargets();
    std::string error;
    if (!createTargets(&error)) return false;
    activeTargetOptions_ = options_;
    return true;
}

void GlRenderer::destroyTargets() {
    auto dropFbo = [](GLuint* fbo, GLuint* tex) {
        if (*fbo) { glDeleteFramebuffers(1, fbo); *fbo = 0; }
        if (*tex) { glDeleteTextures(1, tex); *tex = 0; }
    };
    dropFbo(&hdrFbo_, &hdrTexture_);
    dropFbo(&shadowFbo_, &shadowTexture_);
    for (int i = 0; i < 5; ++i) dropFbo(&bloomFbo_[i], &bloomTexture_[i]);
    if (msaaFbo_) { glDeleteFramebuffers(1, &msaaFbo_); msaaFbo_ = 0; }
    if (msaaColorRb_) { glDeleteRenderbuffers(1, &msaaColorRb_); msaaColorRb_ = 0; }
    if (msaaDepthRb_) { glDeleteRenderbuffers(1, &msaaDepthRb_); msaaDepthRb_ = 0; }
}

void GlRenderer::destroyAll() {
    if (display_ != EGL_NO_DISPLAY && context_ != EGL_NO_CONTEXT) {
        const EGLBoolean madeCurrent = eglMakeCurrent(display_, surface_, surface_, context_);
        if (madeCurrent == EGL_TRUE) {
            destroyTargets();
            const GLuint programs[8] = {forwardProgram_, shadowProgram_, groundProgram_, lineProgram_,
                                        bloomDownProgram_, bloomUpProgram_, compositeProgram_,
                                        debugMagentaProgram_};
            for (GLuint p : programs) if (p) glDeleteProgram(p);
            const GLuint buffers[6] = {cameraUbo_, postUbo_, materialUbo_, vertexVbo_, indexIbo_, groundVbo_};
            for (GLuint b : buffers) if (b) glDeleteBuffers(1, &b);
            if (lineVbo_) glDeleteBuffers(1, &lineVbo_);
            for (int i = 0; i < AF_TEX_COUNT; ++i) if (atlasTexture_[i]) glDeleteTextures(1, &atlasTexture_[i]);
            const GLuint vaos[4] = {avatarVao_, groundVao_, emptyVao_, lineVao_};
            for (GLuint v : vaos) if (v) glDeleteVertexArrays(1, &v);
        } else {
            std::fprintf(stderr, "[AvatarForge] EGL teardown: make-current failed (0x%x); skipping GL deletes\n",
                         static_cast<unsigned int>(eglGetError()));
        }
        /* Unbind before destroying either EGL object. The render thread is the sole owner here. */
        if (eglGetCurrentContext() == context_) {
            if (eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) != EGL_TRUE) {
                std::fprintf(stderr, "[AvatarForge] EGL teardown: unbind failed (0x%x)\n",
                             static_cast<unsigned int>(eglGetError()));
            }
        }
    }
    if (display_ != EGL_NO_DISPLAY) {
        if (surface_ != EGL_NO_SURFACE && eglDestroySurface(display_, surface_) != EGL_TRUE) {
            std::fprintf(stderr, "[AvatarForge] EGL teardown: eglDestroySurface failed (0x%x)\n",
                         static_cast<unsigned int>(eglGetError()));
        }
        if (context_ != EGL_NO_CONTEXT && eglDestroyContext(display_, context_) != EGL_TRUE) {
            std::fprintf(stderr, "[AvatarForge] EGL teardown: eglDestroyContext failed (0x%x)\n",
                         static_cast<unsigned int>(eglGetError()));
        }
        eglTerminate(display_);
    }
    display_ = EGL_NO_DISPLAY;
    surface_ = EGL_NO_SURFACE;
    context_ = EGL_NO_CONTEXT;
}

GlRenderer::~GlRenderer() { destroyAll(); }

} /* namespace */

Renderer* createGlRenderer(ANativeWindow* window, const RendererOptions& options, std::string* error) {
    GlRenderer* r = new GlRenderer();
    if (!r->init(window, options, error)) {
        delete r;
        return nullptr;
    }
    return r;
}

} /* namespace af */
