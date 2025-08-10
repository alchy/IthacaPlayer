# https://cmake.org/download/
# https://trirpi.github.io/posts/developing-audio-plugins-with-juce-and-visual-studio-code/

cd JUCE
cmake -B build
cmake -B build -DJUCE_BUILD_EXTRAS=ON
cmake --build build --target AudioPluginHost

# Visual Studio Code

Build the Project 
- Terminal > Run Build Task (or press Ctrl+Shift+B

Run Without Debugging 
- 