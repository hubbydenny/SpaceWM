project('spacewc', ['cpp', 'c'],
  version : '0.1',
  default_options : [
    'cpp_std=c++20',
    'warning_level=2',
    'buildtype=debugoptimized',
    'cpp_args=-Wall -Wextra -Wno-unused-parameter'
  ])

wlroots        = dependency('wlroots-0.20')
wayland_server = dependency('wayland-server')
wayland_client = dependency('wayland-client')
xkbcommon      = dependency('xkbcommon')
opengl         = dependency('opengl')
glesv2         = dependency('glesv2')
egl            = dependency('egl')

wlroots_cpp_args = ['-fpermissive', '-DWLR_USE_UNSTABLE', '-I' + meson.current_source_dir() + '/src']

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
  dependencies : [wlroots, wayland_server, wayland_client, xkbcommon, opengl, glesv2, egl],
  cpp_args : wlroots_cpp_args,
  install : false)

client_exe = executable('spacewc',
  sources : client_src,
  dependencies : [wlroots, wayland_server, wayland_client, xkbcommon, opengl, glesv2, egl],
  install : false)

run_target('compile_commands',
  command : ['ninja', '-C', meson.current_build_dir(), 'compile_commands.json'])
