#include "Game.hh"

#include <filesystem>
#include <iostream>

int main()
{
    // make sure the executable is run from the correct path
    // (this way we can find all the resources in the data folder)
    if (!std::filesystem::exists("../data"))
    {
        std::cerr << "Working directory must be set to 'bin/'" << std::endl;
        return EXIT_FAILURE;
    }

    // create an instance of the game class
    Game game;

    // run the actual game, i.e. initialize and enter the main loop
    game.run();

    return EXIT_SUCCESS;
}
