/*This source code copyrighted by Lazy Foo' Productions 2004-2024
and may not be redistributed without written permission.*/

// Using SDL, SDL_Renderer, and ImGui
#include <SDL.h>
#include <stdio.h>
#include <string>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <vector>
#include "tinyfiledialogs.h"
extern "C"
{
#include "kvm.h"
}

// Screen dimension constants
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;


int main(int argc, char* args[])
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    // Create SDL Window
    SDL_Window* window = SDL_CreateWindow("INDY-3", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window)
    {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    // Create SDL Renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    // Initialize KVM
    if (kvm_init() != 0) {
        printf("KVM initialization failed.\n");
        return -1;
    }

    // Setup ImGui SDL2 + SDL_Renderer2 bindings
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Main loop
    bool quit = false;
    SDL_Event e;

    bool show_add_window = false;
    bool show_text_input_window = false; // Track if input window is open
    char input_text[128] = ""; // Buffer for user input
    std::string displayed_text = ""; // Stores submitted text
    std::string preset_name = ""; // Store the name of the selected preset  
    std::string temp_preset_name = ""; // Holds preset before submission
    std::vector<std::string> history; // Stores each entry separately
    std::string filename;
    
    while (!quit)
    {
        // Handle SDL events
        while (SDL_PollEvent(&e))
        {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) { quit = true; }
        }

        // Start ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Using ImGui Library here
        // Toolbars
        if (ImGui::BeginMainMenuBar()) //Starts the menu bar
        {
            if (ImGui::BeginMenu("File")) // create drop down menu named file
            {
                if (ImGui::MenuItem("Open"))
                {
                    const char* filter[] = { "*.txt" };  // File filter
                    const char* file = tinyfd_openFileDialog("Open File", "", 1, filter, "Text Files", 0);

                    if (file)
                    {
                        filename = file;  // Assign the file path to the filename variable
                        SDL_RWops* file_handle = SDL_RWFromFile(filename.c_str(), "r");
                        if (file_handle)
                        {
                            displayed_text.clear();
                            Sint64 file_size = SDL_RWsize(file_handle);
                            if (file_size > 0)
                            {
                                // Resize the displayed_text buffer to fit the file content
                                displayed_text.resize(file_size);
                                SDL_RWread(file_handle, &displayed_text[0], 1, file_size);

                                // Ensure the string is null-terminated
                                displayed_text.push_back('\0');  // Append null-terminator at the end

                                printf("Loaded text: %s\n", displayed_text.c_str());
                                // Pass the buffer to ImGui ensuring the length does not include the null terminator
                                ImGui::InputTextMultiline("##CodeText", &displayed_text[0], displayed_text.size(), ImVec2(-FLT_MIN, 200), ImGuiInputTextFlags_AllowTabInput);

                                // Load instructions into KVM
                                if (kvm_load_instructions(filename.c_str()) != 0)
                                {
                                    printf("Error loading instructions into KVM.\n");
                                }
                                else
                                {
                                    printf("Instructions loaded successfully.\n");
                                }
                            }
                            else
                            {
                                printf("Error: File is empty or cannot be read\n");
                            }
                            SDL_RWclose(file_handle);
                        }
                        else
                        {
                            printf("Failed to open file: %s\n", SDL_GetError());
                        }

                    }
                }
        
                if (ImGui::MenuItem("Save")) { /* Handle save action */ }
                if (ImGui::MenuItem("Exit")) { quit = true; }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo")) { /* Handle undo action */ }
                if (ImGui::MenuItem("Redo")) { /* Handle redo action */ }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("Tutorial")) {/*Handle Tutorial action */ }
                if (ImGui::MenuItem("About")) { /* Show about info */ }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }


        // Main UI Layout
        ImGui::SetNextWindowPos(ImVec2(10, 40)); //Set position of the next window(layout) to coordinate
        ImGui::SetNextWindowSize(ImVec2(SCREEN_WIDTH - 20, SCREEN_HEIGHT - 50)); // size of the next window
        // Create a main layout window
        ImGui::Begin("Main Layout", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // Create Two Column laout
        ImGui::Columns(2, nullptr, false);

        // Left Box (Small, Fixed Width)
        ImGui::SetColumnWidth(0, 150);
        ImGui::BeginChild("LeftBox", ImVec2(0, 0), true, // true keeps the box border
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::Text("Code Panel");
                
        if (ImGui::Button("Undo", ImVec2(115, 30))) // Undo button
        {
            if (!history.empty())
            {
                history.pop_back(); // Remove the last added input

                // Rebuild displayed_text from history
                displayed_text.clear();
                for (size_t i = 0; i < history.size(); ++i)
                {
                    if (i > 0) displayed_text.append("\n");
                    displayed_text.append(history[i]);
                }
            }
        }

        if (ImGui::Button("Execute", ImVec2(115, 30)))
        {
            if (!filename.empty())  // Ensure filename is not empty
            {
                printf("Executing file: %s\n", filename.c_str());  // Use .c_str() for printf

                // Attempt to load instructions into KVM and start the VM
                if (kvm_load_instructions(filename.c_str()) != 0)
                {
                    printf("Error loading instructions into KVM.\n");
                }
                else
                {
                    if (kvm_start(-1) != 0)
                    {
                        printf("Error starting KVM VM.\n");
                    }
                }
            }
            else
            {
                printf("No file selected to execute.\n");
            }
        }


         ImGui::EndChild();
        ImGui::NextColumn(); // Move to Right Box

        // Right Box (Large, Fully Expanding)
        ImGui::BeginChild("RightBox", ImVec2(0, 0), true, // true keeps the box border
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::Text("Code Block");
        ImGui::Separator();

              
        // Creates a scrollable box to display the generated code
        ImGui::BeginChild("CodePreview", ImVec2(0, 300), true, ImGuiWindowFlags_HorizontalScrollbar);

        // Ensure the text buffer is large enough for user input
        if (displayed_text.size() <= 1) {
            displayed_text.resize(256);
        }
        // Displays the selected preset name and the user-entered text inside the scrollable area
        ImGui::InputTextMultiline("##CodeText", &displayed_text[0], displayed_text.size(), ImVec2(500, 200), ImGuiInputTextFlags_AllowTabInput);
        

        ImGui::EndChild();


        ImGui::EndChild();
        ImGui::Columns(1); // Exit column mode
        ImGui::End();

        
        // Rendering
        ImGui::Render();
        ImGui::EndFrame();

        SDL_SetRenderDrawColor(renderer, 51, 51, 51, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }
    // Quit KVM when exiting
    kvm_quit();
  

    // Cleanup ImGui
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    // Cleanup SDL
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}