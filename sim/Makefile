SRC=../src/game/canvas.cpp ../src/game/catart.cpp ../src/game/catsprites.cpp ../src/game/catsprites_data.cpp ../src/game/chibi.cpp ../src/game/livewindow.cpp ../src/game/extras.cpp ../src/game/explore.cpp ../src/game/roomdata.cpp ../src/game/gfx.cpp ../src/game/compose.cpp ../src/game/activities.cpp ../src/game/breeds.cpp ../src/game/pet.cpp ../src/game/save.cpp ../src/game/platform.cpp ../src/game/motion.cpp ../src/game/game.cpp ../src/game/home.cpp ../src/game/screens.cpp ../src/game/minigames.cpp ../src/game/netplay.cpp
sim: sim_main.cpp $(SRC)
	g++ -std=c++17 -O1 -g -Wall -Wextra -Wno-uninitialized -fsanitize=address,undefined -o $@ sim_main.cpp $(SRC)

# Regenerates ../src/game/roomdata.cpp from the room painter (only needed when a room design changes).
bakeroom: bakeroom.cpp roomstyles.cpp ../src/game/canvas.cpp
	g++ -std=c++17 -O2 -o $@ bakeroom.cpp roomstyles.cpp ../src/game/canvas.cpp
