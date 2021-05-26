project('wlrect', 'c',
	default_options: [
		'c_std=c11'
	],
	version: '0.4'
)
wayland_client = dependency('wayland-client')
cairo = dependency('cairo')
nwl = subproject('nwl')
nwl_dep = nwl.get_variable('nwl_dep')
srcfiles = [
	'main.c'
]
deps = [
	wayland_client,
	cairo,
	nwl_dep
]
executable('wlrect', srcfiles, dependencies: deps, install:true)
