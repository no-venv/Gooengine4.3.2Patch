import build_scripts

option_text="""

Build Menu
[1] Compile Goo Engine
[2] Create AppImage

"""
option = int(input(option_text))
match option:
    case 1:
        build_scripts.run_compile()
        pass
    case 2:
        build_scripts.run_build_appimage()
        pass
    case _:
        print("Invalid option")
