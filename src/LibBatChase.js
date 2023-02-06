mergeInto(LibraryManager.library, {
    upload_flipped: function(img) {
        GLctx.pixelStorei(0x9240/*GL_UNPACK_FLIP_Y_WEBGL*/, true);
        GLctx.texImage2D(0xDE1/*GL_TEXTURE_2D*/, 0, 0x1908/*GL_RGBA*/, 0x1908/*GL_RGBA*/, 0x1401/*GL_UNSIGNED_BYTE*/, img);
        GLctx.pixelStorei(0x9240/*GL_UNPACK_FLIP_Y_WEBGL*/, false);
    },
    load_image__deps: ['upload_flipped'],
    load_image: function(glTex, url, outWidth, outHeight) {
        let img = new Image();
        img.onload = () => {
            HEAPU32[outWidth>>2] = img.width;
            HEAPU32[outHeight>>2] = img.height;
            GLctx.bindTexture(0xDE1/*GL_TEXTURE_2D*/, GL.textures[glTex]);
            _upload_flipped(img);
        };
        img.src = UTF8ToString(url);
    },
    // \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
}); // line 65
