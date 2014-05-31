 #!/bin/sh

echo -e "[${_G} Opengl-es acceleration module setting. ${C_}]"
if grep "yagl=1" /proc/cmdline ; then
        echo -e "[${_G} Emulator support gles hw acceleration. ${C_}]"
        echo -e "[${_G} Change permission of /dev/yagl. ${C_}]"
        chmod 666 /dev/yagl
        echo -e "[${_G} Apply to use hw gles library. ${C_}]"
               ln -s -f /usr/lib/yagl/libEGL.so.1.0 /usr/lib/libEGL.so
               ln -s -f /usr/lib/yagl/libEGL.so.1.0 /usr/lib/libEGL.so.1
               ln -s -f /usr/lib/yagl/libGLESv1_CM.so.1.0 /usr/lib/libGLESv1_CM.so
               ln -s -f /usr/lib/yagl/libGLESv1_CM.so.1.0 /usr/lib/libGLESv1_CM.so.1
               ln -s -f /usr/lib/yagl/libGLESv2.so.1.0 /usr/lib/libGLESv2.so
               ln -s -f /usr/lib/yagl/libGLESv2.so.1.0 /usr/lib/libGLESv2.so.1
               systemctl set-environment ELM_ENGINE=gl
else
        echo -e "[${_G} Emulator does not support gles hw acceleration. ${C_}]"
               echo -e "[${_G} Apply to use gles dummy library. ${C_}]"
               ln -s -f /usr/lib/dummy/libEGL_dummy.so /usr/lib/libEGL.so
               ln -s -f /usr/lib/dummy/libEGL_dummy.so /usr/lib/libEGL.so.1
               ln -s -f /usr/lib/dummy/libGLESv1_dummy.so /usr/lib/libGLESv1_CM.so
               ln -s -f /usr/lib/dummy/libGLESv1_dummy.so /usr/lib/libGLESv1_CM.so.1
               ln -s -f /usr/lib/dummy/libGLESv2_dummy.so /usr/lib/libGLESv2.so
               ln -s -f /usr/lib/dummy/libGLESv2_dummy.so /usr/lib/libGLESv2.so.1
fi
