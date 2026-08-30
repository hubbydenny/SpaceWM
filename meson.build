project('spacewc', ['cpp', 'c'],
  version : '0.1',
  default_options : [
    'cpp_std=c++20',
    'warning_level=2',
    'buildtype=debugoptimized',
    'cpp_args=-Wall -Wextra -Wno-unused-parameter',
    'pkg_config_path=/usr/lib/pkgconfig:/usr/share/pkgconfig'
  ])

wlroots_inc = ['-I' + meson.current_source_dir() + '/src',
               '-I/usr/include/wlroots-0.20']
wlroots_lib = ['-lwlroots-0.20']

wlroots_dep = dependency('wlroots-0.20')
wayland_server = dependency('wayland-server')
wayland_client = dependency('wayland-client')
xkbcommon      = dependency('xkbcommon')
opengl         = dependency('opengl')
glesv2         = dependency('glesv2')

cpp = meson.get_compiler('cpp')
egl_lib = cpp.find_library('EGL', dirs : ['/usr/lib'])

client_src = [
  'src/main_client.cpp',
  'src/WaylandClient.cpp',
  'src/xdg-shell-client-protocol.c',
]

comp_src = [
  'src/main_comp.cpp',
  'src/WaylandComp.cpp',
  'src/animations.cpp',
]

comp_exe = executable('spacewc-comp',
  sources : comp_src,
  dependencies : [wlroots_dep, wayland_server, wayland_client, xkbcommon, opengl, glesv2, egl_lib],
  link_args : wlroots_lib,
  link_depends : '/usr/lib/libwlroots-0.20.so',
  cpp_args : ['-fpermissive', '-DWLR_USE_UNSTABLE'] + wlroots_inc,
  install : false)

client_exe = executable('spacewc',
  sources : client_src,
  dependencies : [wayland_client, xkbcommon],
  install : false)

run_target('compile_commands',
  command : ['ninja', '-C', meson.current_build_dir(), 'compile_commands.json'])
