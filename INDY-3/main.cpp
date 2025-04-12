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

extern std::vector<std::string> history;


// Screen dimension constants
int SCREEN_WIDTH = 1025;
int SCREEN_HEIGHT = 650;


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
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window)
    {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }
    SDL_GetWindowSize(window, &SCREEN_WIDTH, &SCREEN_HEIGHT);

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
   
    std::string filename;
    std::vector<std::string> history;
    bool is_code_loaded_from_file = false;
    
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
                if (ImGui::MenuItem("New Project")) {
                    is_code_loaded_from_file = false;
                    displayed_text.clear();
                }
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

                                // Store the loaded file content in history
                                history.push_back(displayed_text);

                                // Set the flag to indicate that the code was loaded from a file
                                is_code_loaded_from_file = true;
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
        
                if (ImGui::MenuItem("Save"))
                {
                    if (is_code_loaded_from_file && !filename.empty())
                    {
                        // Save (overwrite) to the loaded file
                        SDL_RWops* file_handle = SDL_RWFromFile(filename.c_str(), "w");
                        if (file_handle)
                        {
                            SDL_RWwrite(file_handle, displayed_text.c_str(), 1, displayed_text.length());
                            SDL_RWclose(file_handle);
                            printf("Saved to existing file: %s\n", filename.c_str());
                        }
                        else
                        {
                            printf("Failed to save file: %s\n", SDL_GetError());
                        }
                    }
                    else
                    {
                        // Ask for a new file name
                        const char* save_path = tinyfd_saveFileDialog("Save File", "untitled.txt", 0, nullptr, nullptr);
                        if (save_path)
                        {
                            SDL_RWops* file_handle = SDL_RWFromFile(save_path, "w");
                            if (file_handle)
                            {
                                SDL_RWwrite(file_handle, displayed_text.c_str(), 1, displayed_text.length());
                                SDL_RWclose(file_handle);
                                filename = save_path;
                                is_code_loaded_from_file = true;  // Now it becomes a loaded file
                                printf("Saved as new file: %s\n", filename.c_str());
                            }
                            else
                            {
                                printf("Failed to save file: %s\n", SDL_GetError());
                            }
                        }
                    }
                }
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
                // Remove the last added input from history
                history.pop_back();

                // Rebuild displayed_text from history
                displayed_text.clear();
                if (!history.empty())
                {
                    displayed_text = history.back();  // Set displayed_text to the last state in history
                }
                else
                {
                    displayed_text.clear();  // If no history, clear the text
                }

                // Reset filename (clear if it was linked to an invalid file)
                filename.clear();

                // If the code was loaded from a file, reset flag
                if (is_code_loaded_from_file)
                {
                    is_code_loaded_from_file = false;
                }
            }
        }

        if (ImGui::Button("Execute", ImVec2(115, 30)))
        {
           
            if (!displayed_text.empty())  // Case 2: User typed code in the code block
            {
                printf("Executing typed code:\n%s\n", displayed_text.c_str());

                // Sanitize the displayed text: Remove any non-printable characters or null bytes
                std::string sanitized_text;
                for (char c : displayed_text)
                {
                    if (isprint(c) || c == '\n' || c == '\t')  // Only include printable characters
                    {
                        sanitized_text += c;
                    }
                }

                // Set the temporary file name as "temp_code"
                std::string temp_filename = "temp_code";  // No .txt extension
                SDL_RWops* file_handle = SDL_RWFromFile((temp_filename+".txt").c_str(), "w");

                if (file_handle)
                {
                    // Write the sanitized code to the file
                    Sint64 file_size = sanitized_text.size();
                    SDL_RWwrite(file_handle, (sanitized_text).c_str(), 1, file_size);
                    SDL_RWclose(file_handle);

                    printf("Temporary file created: %s\n", temp_filename.c_str());

                    // Initialize KVM
                    if (kvm_init() != 0) {
                        printf("KVM initialization failed.\n");
                        return -1;
                    }

                    // Now load the temporary file into KVM
                    if (kvm_load_instructions(temp_filename.c_str()) != 0)
                    {
                        printf("Error loading instructions into KVM.\n");
                        return -1;
                    }
                    if (kvm_start(-1) != 0)
                    {
                        printf("Error starting KVM VM.\n");
                        return -1;
                    }
                    kvm_quit(); 
                }
                else
                {
                    printf("Error creating temporary file: %s\n", temp_filename.c_str());
                }
            }
            else
            {
                printf("No code to execute.\n");
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
        ImGui::BeginChild("CodePreview", ImVec2(0, SCREEN_HEIGHT), true, ImGuiWindowFlags_HorizontalScrollbar);

        // Ensure the text buffer is large enough for user input
        if (displayed_text.size() <= 1) {
            displayed_text.resize(256);
        }
        // Displays the selected preset name and the user-entered text inside the scrollable area
        ImGui::InputTextMultiline("##CodeText", &displayed_text[0], displayed_text.size(), ImVec2(SCREEN_WIDTH-250, SCREEN_HEIGHT - 100), ImGuiInputTextFlags_AllowTabInput);
        

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