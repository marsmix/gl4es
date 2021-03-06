#include "line.h"

void gl4es_glLineStipple(GLuint factor, GLushort pattern) {
    if(glstate->list.active) {
        if (glstate->list.compiling) {
            NewStage(glstate->list.active, STAGE_LINESTIPPLE);
            glstate->list.active->linestipple_op = 1;
            glstate->list.active->linestipple_factor = factor;
            glstate->list.active->linestipple_pattern = pattern;
            return;
        } else if(glstate->list.pending) flush();
    }
    if(pattern!=glstate->linestipple.pattern || factor!=glstate->linestipple.factor || !glstate->linestipple.texture) {
        glstate->linestipple.factor = factor;
        glstate->linestipple.pattern = pattern;
        for (int i = 0; i < 16; i++) {
            glstate->linestipple.data[i] = (pattern >> i) & 1 ? 255 : 0;
        }

        gl4es_glPushAttrib(GL_TEXTURE_BIT);
        if (! glstate->linestipple.texture)
            gl4es_glGenTextures(1, &glstate->linestipple.texture);

        gl4es_glBindTexture(GL_TEXTURE_2D, glstate->linestipple.texture);
        gl4es_glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl4es_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        gl4es_glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA,
            16, 1, 0, GL_ALPHA, GL_UNSIGNED_BYTE, glstate->linestipple.data);
        gl4es_glPopAttrib();
        noerrorShim();
    }
}
void glLineStipple(GLuint factor, GLushort pattern) AliasExport("gl4es_glLineStipple");

void bind_stipple_tex() {
    gl4es_glBindTexture(GL_TEXTURE_2D, glstate->linestipple.texture);
}

GLfloat *gen_stipple_tex_coords(GLfloat *vert, int length) {
    // generate our texture coords
    GLfloat *tex = (GLfloat *)malloc(length * 4 * sizeof(GLfloat));
    GLfloat *texPos = tex;
    GLfloat *vertPos = vert;

    GLfloat x1, x2, y1, y2;
    GLfloat len;
    for (int i = 0; i < length / 2; i++) {
        x1 = *vertPos++;
        y1 = *vertPos++;
        vertPos++; // z
        vertPos++; // w
        x2 = *vertPos++;
        y2 = *vertPos++;
        vertPos++;
        vertPos++;

        len = sqrtf(powf(x2-x1, 2) + powf(y2-y1, 2)) / glstate->linestipple.factor * 16;

        *texPos++ = 0;
        *texPos++ = 0;
        *texPos++ = 0;
        *texPos++ = 1;
        *texPos++ = len;
        *texPos++ = 0;
        *texPos++ = 0;
        *texPos++ = 1;
    }
    return tex;
}
