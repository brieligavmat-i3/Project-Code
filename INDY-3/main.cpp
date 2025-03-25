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

extern "C" {
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

    
    while (!quit)
    {
        // Handle SDL events
        while (SDL_PollEvent(&e))
        {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) quit = true;
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
                    const char* filter[] = { "*.txt" };
                    const char* filename = tinyfd_openFileDialog("Open File", "", 1, filter, "Text Files", 0);

                    if (filename)
                    {
                        SDL_RWops* file = SDL_RWFromFile(filename, "r");
                        if (file)
                        {
                            displayed_text.clear(); // Clear existing text

                            Sint64 file_size = SDL_RWsize(file);
                            if (file_size > 0)
                            {
                                static char buffer[4096] = "";

                                std::vector<char> temp_buffer(file_size + 1, '\0');
                                SDL_RWread(file, temp_buffer.data(), 1, file_size);

                                // Now copy the contents to the buffer
                                strncpy_s(buffer, sizeof(buffer), temp_buffer.data(), _TRUNCATE);
                                buffer[sizeof(buffer) - 1] = '\0'; // Ensure null-termination

                                // Update displayed_text
                                displayed_text = buffer; // Use the buffer directly for simplicity

                                // Log the loaded text
                                printf("Loaded text: %s\n", displayed_text.c_str());

                                // In the UI code, make sure to render displayed_text
                                ImGui::InputTextMultiline("##CodeText", &displayed_text[0], displayed_text.size() + 1, ImVec2(-FLT_MIN, 200));


                                // Log to see if data is being read
                                printf("File successfully loaded: %s\n", filename);
                            }
                            else
                            {
                                printf("Error: File is empty or cannot be read\n");
                            }
                            SDL_RWclose(file);
                        }
                        else
                        {
                            printf("Failed to open file: %s\n", SDL_GetError());
                        }
                    }
                    else
                    {
                        printf("Failed to select file or user canceled the dialog.\n");
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
        // Big "Add" button in the top-left 
        if (ImGui::Button("Add", ImVec2(115, 50))) // Button position
        {
            show_add_window = true;
        }
        
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


        ImGui::EndChild();
        ImGui::NextColumn(); // Move to Right Box

        // Right Box (Large, Fully Expanding)
        ImGui::BeginChild("RightBox", ImVec2(0, 0), true, // true keeps the box border
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::Text("Code Block");
        ImGui::Separator();



        // Display section for generated code
        
        // Creates a scrollable box to display the generated code
        ImGui::BeginChild("CodePreview", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);

        // Displays the selected preset name and the user-entered text inside the scrollable area
        ImGui::InputTextMultiline("##CodeText", &displayed_text[0], displayed_text.size() + 1, ImVec2(500, 200)); 

        ImGui::EndChild();


        ImGui::EndChild();
        ImGui::Columns(1); // Exit column mode
        ImGui::End();

        // "Add" button pop-up window
        if (show_add_window)
        {
            ImGui::Begin("Add", &show_add_window, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::Text("Select Instruction:");

            // Capture which button was clicked and set preset_name
            if (ImGui::Button("NOP")) { temp_preset_name = "NOP"; show_text_input_window = true; }
            if (ImGui::Button("Preset 2")) { temp_preset_name = "Preset 2"; show_text_input_window = true; }
            if (ImGui::Button("Preset 3")) { temp_preset_name = "Preset 3"; show_text_input_window = true; }


            if (ImGui::Button("Close")) {
                show_add_window = false;
            }

            ImGui::End();
        }

        // Show input window when the button is clicked
        if (show_text_input_window)
        {
            // Create a window with title, when x is clicked closes
            ImGui::Begin("Enter Input", &show_text_input_window, ImGuiWindowFlags_AlwaysAutoResize);

            // Show input prompt
            ImGui::Text("Enter your Input: ");
            // Input box, input_text to store text
            ImGui::InputText("##input", input_text, IM_ARRAYSIZE(input_text));

            //Submit button
            if (ImGui::Button("Submit"))
            {
                if (!temp_preset_name.empty()) // Ensure preset is selected
                {
                    std::string new_entry = temp_preset_name + " " + input_text;

                    history.push_back(new_entry); // Store entry in history 

                    if (displayed_text.empty())
                    {
                        displayed_text = new_entry; // First entry, no newline
                    }
                    else
                    {
                        displayed_text.append("\n").append(new_entry); // Append with newline only after the first entry
                    }

                    temp_preset_name.clear();  // Reset preset
                    input_text[0] = '\0';   // Clear input text

                    show_add_window = false; 
                }
                show_text_input_window = false;
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                show_text_input_window = false;
            }

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        ImGui::EndFrame();

        SDL_SetRenderDrawColor(renderer, 51, 51, 51, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

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