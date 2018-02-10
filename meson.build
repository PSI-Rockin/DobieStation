project('dobiestation', 'cpp', default_options : ['cpp_std=c++11'])
qt5 = import('qt5')
qt5dep = dependency('qt5', modules: ['Widgets'])

src = ['src/qt/main.cpp',
      'src/core/emotion.cpp',
      'src/core/emulator.cpp',
      'src/core/emotioninterpreter.cpp',
      'src/core/cop0.cpp',
      'src/core/cop1.cpp',
      'src/core/emotion_mmi.cpp',
      'src/core/bios_hle.cpp',
      'src/core/vu0.cpp',
      'src/core/emotion_special.cpp',
      'src/core/gif.cpp',
      'src/core/gs.cpp',
      'src/core/dmac.cpp',
      'src/qt/emuwindow.cpp',
      'src/core/gscontext.cpp',
      'src/core/emotiondisasm.cpp',
      'src/core/emotionasm.cpp',
      'src/core/emotion_fpu.cpp']

headers = ['src/core/emotion.hpp',
          'src/core/emulator.hpp',
          'src/core/emotioninterpreter.hpp',
          'src/core/cop0.hpp',
          'src/core/cop1.hpp',
          'src/core/bios_hle.hpp',
          'src/core/vu0.hpp',
          'src/core/gif.hpp',
          'src/core/gs.hpp',
          'src/core/dmac.hpp',
          'src/qt/emuwindow.hpp',
          'src/core/gscontext.hpp',
          'src/core/emotiondisasm.hpp',
          'src/core/emotionasm.hpp']

moc_files = qt5.preprocess(moc_headers: headers)

executable('dobiestation', src, moc_files, dependencies : qt5dep, install : true)
