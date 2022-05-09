project('wlrect', 'c',
	default_options: [
		'c_std=c11'
	],
	version: '0.4'
)
wayland_client = dependency('wayland-client')
cairo = dependency('cairo')
nwlsub = subproject('nwl', required: false)
if nwlsub.found()
	nwl = nwlsub.get_variable('nwl_dep')
else
	nwl = dependency('nwl', required: true)
endif
srcfiles = [
	'main.c'
]
deps = [
	wayland_client,
	cairo,
	nwl
]
executable('wlrect', srcfiles, dependencies: deps, install:true)
