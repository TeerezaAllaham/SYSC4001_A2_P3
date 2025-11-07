/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include "interrupts_<101289630>_<101287549>.hpp"
#include <fstream>
#include <tuple>
#include <iostream>
#include <vector>
#include <string>
std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, std::vector<std::string> vectors, std::vector<int> delays, std::vector<external_file> external_files, PCB current, std::vector<PCB> wait_queue) {

    std::string trace;      //!< string to store single line of trace file
    std::string execution = "";  //!< string to accumulate the execution output
    std::string system_status = "";  //!< string to accumulate the system status output
    int current_time = time;

    //parse each line of the input trace file. 'for' loop to keep track of indices.
    for(size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];

        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if(activity == "CPU") { //As per Assignment 1
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;
        } else if(activity == "SYSCALL") { //As per Assignment 1
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR (ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "END_IO") {
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            current_time = time;
            execution += intr;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR(ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "FORK") {
            // Trigger the interrupt boilerplate for vector 2 and implements FORK system call
            // This simulates switching to kernel mode, saving context, and loading ISR address
            auto [intr, time] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time;
            
            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your FORK output here
            
            // Log FORK ISR activity
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", FORK ISR triggered\n";
            current_time += duration_intr;

            // Create child PCB
            PCB child(current.PID + 1, current.PID, current.program_name, current.size, -1);

            // Allocate memory for the child
            if (!allocate_memory(&child)) {
                std::cerr << "ERROR! Memory allocation failed for child process.\n";
            }

            // Add parent to waiting queue
            wait_queue.push_back(current);

            // Switch context to child process (child runs first)
            current = child;

            // Log system status
            system_status += "time: " + std::to_string(current_time) + "; current trace: FORK, " + std::to_string(duration_intr) + "\n";
            system_status += print_PCB(current, wait_queue) + "\n";
            
            ///////////////////////////////////////////////////////////////////////////////////////////

            //The following loop helps you do 2 things:
            // * Collect the trace of the chile (and only the child, skip parent)
            // * Get the index of where the parent is supposed to start executing from
            std::vector<std::string> child_trace;
            bool skip = true;
            bool exec_flag = false;
            int parent_index = 0;

            for(size_t j = i; j < trace_file.size(); j++) {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);
                if(skip && _activity == "IF_CHILD") {
                    skip = false;
                    continue;
                } else if(_activity == "IF_PARENT"){
                    skip = true;
                    parent_index = j;
                    if(exec_flag) {
                        break;
                    }
                } else if(skip && _activity == "ENDIF") {
                    skip = false;
                    continue;
                } else if(!skip && _activity == "EXEC") {
                    skip = true;
                    child_trace.push_back(trace_file[j]);
                    exec_flag = true;
                }

                if(!skip) {
                    child_trace.push_back(trace_file[j]);
                }
            }
            i = parent_index;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the child's trace, run the child (HINT: think recursion)

            // Recursively simulate the child's trace
            if (!child_trace.empty()) {
                auto [child_exec, child_status, child_time] = simulate_trace(
                    child_trace,
                    current_time,
                    vectors,
                    delays,
                    external_files,
                    current,
                    wait_queue
                );

                // Append the child’s logs to the parent’s
                execution += child_exec;
                system_status += child_status;

                // Update simulation time after child finishes
                current_time = child_time;
            }
            
            ///////////////////////////////////////////////////////////////////////////////////////////


        } else if(activity == "EXEC") {
            // Trigger interrupt boilerplate for vector 3
            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            execution += intr;
            current_time = time;
            execution += intr;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your EXEC output here
             // Log that EXEC interrupt has started
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", EXEC system call initiated\n";
            current_time += duration_intr;

            // Get size of program from external_files table
            unsigned int program_size = get_size(program_name, external_files);
            if(program_size == static_cast<unsigned int>(-1)) {
                std::cerr << "ERROR! Program " << program_name << " not found in external files.\n";
                continue; // skip this EXEC
            }

            // Allocate memory: find an empty partition where the program fits
            PCB new_process(current.PID, current.PID, program_name, program_size, -1);
            if(!allocate_memory(&new_process)) {
                std::cerr << "ERROR! No suitable memory partition available for " << program_name << "\n";
                continue; // skip this EXEC
            }

            // Simulate the loader: 15ms per MB
            int load_time = program_size * 15;
            execution += std::to_string(current_time) + ", " + std::to_string(load_time) + ", Loading program " + program_name + " into Partition " + std::to_string(new_process.partition_number) + "\n";
            current_time += load_time;

            // Replace the current PCB's process image with the new program
            free_memory(&current);              // free previous memory
            current = new_process;              // update current PCB

            // IRET to return from interrupt
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

            // Update system status
            system_status += "time: " + std::to_string(current_time) + "; EXEC invoked -> New program loaded: " + current.program_name + " in Partition " + std::to_string(current.partition_number) + "\n";
            system_status += print_PCB(current, wait_queue) + "\n";
            
            ///////////////////////////////////////////////////////////////////////////////////////////


            std::ifstream exec_trace_file(program_name + ".txt");
            std::vector<std::string> exec_traces;
            std::string exec_trace;
            while(std::getline(exec_trace_file, exec_trace)) {
                exec_traces.push_back(exec_trace);
            }

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the exec's trace (i.e. trace of external program), run the exec (HINT: think recursion)
        if(exec_trace_file.is_open()) {
            std::string exec_trace;

            // Read the external program's trace into a vector
            while(std::getline(exec_trace_file, exec_trace)) {
                exec_traces.push_back(exec_trace);
            }
            exec_trace_file.close();

            // Recursively simulate the exec program trace
            auto [exec_output, exec_status, exec_end_time] = simulate_trace(
                exec_traces,         // the external program trace
                current_time,        // start time
                vectors,
                delays,
                external_files,
                current,             // current PCB
                wait_queue
            );

            // Append recursive output to main execution logs
            execution += exec_output;
            system_status += exec_status;
            current_time = exec_end_time;
        }



                ///////////////////////////////////////////////////////////////////////////////////////////

                break; //Why is this important? (answer in report)

            }
        }

    return {execution, system_status, current_time};
}

int main(int argc, char** argv) {

    //vectors is a C++ std::vector of strings that contain the address of the ISR
    //delays  is a C++ std::vector of ints that contain the delays of each device
    //the index of these elemens is the device number, starting from 0
    //external_files is a C++ std::vector of the struct 'external_file'. Check the struct in 
    //interrupt.hpp to know more.
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    //Just a sanity check to know what files you have
    print_external_files(external_files);

    //Make initial PCB (notice how partition is not assigned yet)
    PCB current(0, -1, "init", 1, 6);
    // Manually assign partition 6 in memory[]
    memory[5].code = current.program_name;  // 

    std::vector<PCB> wait_queue;

    /******************ADD YOUR VARIABLES HERE*************************/
    int start_time = 0;                        // Starting time for simulation
    std::string execution_output = "";         // Placeholder for execution log
    std::string system_status_output = "";     // Placeholder for system status log
    int end_time = 0;                          // Track end time of simulation

    /******************************************************************/

    //Converting the trace file into a vector of strings.
    std::vector<std::string> trace_file;
    std::string trace;
    while(std::getline(input_file, trace)) {
        trace_file.push_back(trace);
    }

    auto [execution, system_status, _] = simulate_trace(   trace_file, 
                                            0, 
                                            vectors, 
                                            delays,
                                            external_files, 
                                            current, 
                                            wait_queue);

    input_file.close();

    write_output(execution, "execution.txt");
    write_output(system_status, "system_status.txt");

    return 0;
}
