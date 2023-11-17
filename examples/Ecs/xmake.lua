
target("NzImgui-ecs-demo")
	set_group("Examples")
	add_files("main.cpp")
	add_packages("nazara")
	add_deps("NazaraImgui")
	set_rundir(".")