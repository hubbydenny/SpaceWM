project('spacewc', ['cpp', 'c'],
  version : '0.1',
  default_options : [
    'cpp_std=c++23',
    'warning_level=2',
    'buildtype=debugoptimized',
    'cpp_args=-Wall -Wextra -Wpedantic'
  ])

wlroots        = dependency('wlroots-0.20')
wayland_server = dependency('wayland-server')
wayland_client = dependency('wayland-client')
xkbcommon      = dependency('xkbcommon')

executable('spacewc',
  sources : [
    'src/main.cpp',
    'src/WaylandClient.cpp',
    'src/xdg-shell-client-protocol.c'
  ],
  dependencies : [wlroots, wayland_server, wayland_client, xkbcommon],
  install : false)

run_target('compile_commands',
  command : ['ninja', '-C', meson.current_build_dir(), 'compile_commands.json'])
