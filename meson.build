project(
  'tedit',
  'cpp',
  version: '2.2.0',
  default_options: [
    'cpp_std=c++17',
    'warning_level=3',
    'buildtype=release',
  ],
)

cpp = meson.get_compiler('cpp')

add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'cpp')

if get_option('b_ndebug') != 'true'
  add_project_arguments('-fno-omit-frame-pointer', language: 'cpp')
endif

lua_dep = dependency('', required: false)
foreach lua_name : ['lua5.4', 'lua5.3', 'lua', 'luajit']
  if not lua_dep.found()
    lua_dep = dependency(lua_name, required: false)
  endif
endforeach

if not lua_dep.found()
  lua_dep = cpp.find_library('lua5.4', required: true)
  add_project_arguments('-I/usr/include/lua5.4', language: 'cpp')
endif

dl_dep = cpp.find_library('dl', required: false)
m_dep = cpp.find_library('m', required: false)

executable(
  'tedit',
  'src/tedit.cpp',
  dependencies: [lua_dep, dl_dep, m_dep],
  install: true,
)

install_man('mandoc/tedit.1')
