mergeInto(LibraryManager.library, {
    upload_flipped: function (img) {
        const GL_UNPACK_FLIP_Y_WEBGL = 0x9240;
        const GL_TEXTURE_2D = 0xDE1;
        const GL_RGBA = 0x1908;
        const GL_UNSIGNED_BYTE = 0x1401;

        GLctx.pixelStorei(GL_UNPACK_FLIP_Y_WEBGL, true);
        GLctx.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, img);
        GLctx.pixelStorei(GL_UNPACK_FLIP_Y_WEBGL, false);
    },
    load_image__deps: ['upload_flipped'],
    load_image: function (glTex, url, outWidth, outHeight) {
        let img = new Image();
        img.onload = () => {
            const GL_TEXTURE_2D = 0xDE1;

            HEAPU32[outWidth >> 2] = img.width;
            HEAPU32[outHeight >> 2] = img.height;
            GLctx.bindTexture(GL_TEXTURE_2D, GL.textures[glTex]);
            _upload_flipped(img);
        };
        img.src = UTF8ToString(url);
    },
    preloaded_audio: {},
    preload_audio__deps: ['preloaded_audio'],
    preload_audio: function (id, url) {
        let audio = new Audio(UTF8ToString(url));
        // TODO hardcoded lower volume
        audio.volume = 0.5;
        _preloaded_audio[id] = audio;
        audio.preload = 'auto';
    },
    play_audio: function (id, loop) {
        let audio = _preloaded_audio[id];
        audio.loop = !!loop;
        audio.play()
            .catch(e => {
                let deferPlay = () => {
                    audio.play()
                        .then(() => {
                            document.body.removeEventListener('keydown', deferPlay);
                        });
                };

                document.body.addEventListener('keydown', deferPlay);
            });
    },
    loadedFonts: {},
    load_font__deps: ['loadedFonts'],
    load_font: function (fontId, url) {
        new FontFace(`font${fontId}`, `url(${UTF8ToString(url)})`).load().then(face => {
            document.fonts.add(face);
            _loadedFonts[`font${fontId}`] = 1;
        });
    },
    upload_unicode_char_to_texture__deps: ['upload_flipped'],
    upload_unicode_char_to_texture: function (fontId, unicodeChar, fontSize) {
        if (!_loadedFonts[`font${fontId}`])
            return 0;
        let canvas = document.createElement('canvas');
        canvas.width = canvas.height = fontSize;
        let ctx = canvas.getContext('2d');
        ctx.fillStyle = 'black';
        ctx.globalCompositeOperator = 'copy';
        ctx.globalAlpha = 0;
        ctx.fillRect(0, 0, canvas.width, canvas.height);
        ctx.globalAlpha = 1;
        ctx.fillStyle = 'white';
        ctx.font = fontSize + `px font${fontId}`;
        ctx.fillText(String.fromCharCode(unicodeChar), 0, canvas.height);
        _upload_flipped(canvas);
        return 1;
    },
});
