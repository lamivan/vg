//
//  genome_state.cpp
//
//  Unit tests for SnarlState, GenomeState and related functions
//

#include <stdio.h>
#include <iostream>
#include <set>
#include "catch.hpp"
#include "../json2pb.h"
#include "../genotypekit.hpp"
#include "../genome_state.hpp"


namespace vg {
namespace unittest {

using namespace std;

TEST_CASE("SnarlState can hold and manipulate haplotypes", "[snarlstate][genomestate]") {
    

    // This graph will have a snarl from 1 to 8, a snarl from 2 to 7,
    // and a snarl from 3 to 5, all nested in each other.
    VG graph;
        
    Node* n1 = graph.create_node("GCA");
    Node* n2 = graph.create_node("T");
    Node* n3 = graph.create_node("G");
    Node* n4 = graph.create_node("CTGA");
    Node* n5 = graph.create_node("GCA");
    Node* n6 = graph.create_node("T");
    Node* n7 = graph.create_node("G");
    Node* n8 = graph.create_node("CTGA");
    
    Edge* e1 = graph.create_edge(n1, n2);
    Edge* e2 = graph.create_edge(n1, n8);
    Edge* e3 = graph.create_edge(n2, n3);
    Edge* e4 = graph.create_edge(n2, n6);
    Edge* e5 = graph.create_edge(n3, n4);
    Edge* e6 = graph.create_edge(n3, n5);
    Edge* e7 = graph.create_edge(n4, n5);
    Edge* e8 = graph.create_edge(n5, n7);
    Edge* e9 = graph.create_edge(n6, n7);
    Edge* e10 = graph.create_edge(n7, n8);
    
    // Work out its snarls
    CactusSnarlFinder bubble_finder(graph);
    SnarlManager snarl_manager = bubble_finder.find_snarls();
    
    // Get the top snarl
    const Snarl* top_snarl = snarl_manager.top_level_snarls().at(0);
    
    // Make sure it's what we expect.
    REQUIRE(top_snarl->start().node_id() == 1);
    REQUIRE(top_snarl->end().node_id() == 8);
    
    // And get its net graph
    NetGraph net_graph = snarl_manager.net_graph_of(top_snarl, &graph, true);
    
    // Make a SnarlState for it
    SnarlState state(&net_graph);
    
    SECTION("The state starts empty") {
        REQUIRE(state.size() == 0);
    }
    
    SECTION("A haplotype with lane numbers can be added in lane 0 when the SnarlState is empty") {
        // Make a haplotype
        vector<pair<handle_t, size_t>> annotated_haplotype;
        
        // Say we go 1, 2 (which is a child snarl), 8
        annotated_haplotype.emplace_back(net_graph.get_handle(1, false), 0);
        annotated_haplotype.emplace_back(net_graph.get_handle(2, false), 0);
        annotated_haplotype.emplace_back(net_graph.get_handle(8, false), 0);
        
        // Put it in the state
        state.insert(annotated_haplotype);
        
        SECTION("The state now has 1 haplotype") {
            REQUIRE(state.size() == 1);
        }
        
        SECTION("The haplotype can be traced back again") {
            vector<pair<handle_t, size_t>> recovered;
            
            state.trace(0, false, [&](const handle_t& visit, size_t local_lane) {
                recovered.emplace_back(visit, local_lane);
            });
            
            REQUIRE(recovered == annotated_haplotype);
        }
        
        SECTION("The haplotype can be traced in reverse") {
            vector<pair<handle_t, size_t>> recovered;
            
            state.trace(0, true, [&](const handle_t& visit, size_t local_lane) {
                recovered.emplace_back(net_graph.flip(visit), local_lane);
            });
            
            reverse(recovered.begin(), recovered.end());
            
            REQUIRE(recovered == annotated_haplotype);
        }
        
        SECTION("A haplotype with lane numbers can be inserted before an existing haplotype") {
            
            vector<pair<handle_t, size_t>> hap2;
            // Say we go 1, 8 directly in lane 0
            hap2.emplace_back(net_graph.get_handle(1, false), 0);
            hap2.emplace_back(net_graph.get_handle(8, false), 0);
            
            // Put it in the state
            state.insert(hap2);
            
            SECTION("The state now has 2 haplotypes") {
                REQUIRE(state.size() == 2);
            }
            
            SECTION("The new haplotype can be traced back again") {
                vector<pair<handle_t, size_t>> recovered;
                
                state.trace(0, false, [&](const handle_t& visit, size_t local_lane) {
                    recovered.emplace_back(visit, local_lane);
                });
                
                REQUIRE(recovered == hap2);
            }
            
            SECTION("The old haplotype can be traced back again") {
                vector<pair<handle_t, size_t>> recovered;
                
                state.trace(1, false, [&](const handle_t& visit, size_t local_lane) {
                    recovered.emplace_back(visit, local_lane);
                });
                
                REQUIRE(recovered.size() == 3);
                REQUIRE(recovered[0].first == annotated_haplotype[0].first);
                REQUIRE(recovered[0].second == annotated_haplotype[0].second + 1);
                // The second mapping should not get bumped up.
                REQUIRE(recovered[1].first == annotated_haplotype[1].first);
                REQUIRE(recovered[1].second == annotated_haplotype[1].second);
                REQUIRE(recovered[2].first == annotated_haplotype[2].first);
                REQUIRE(recovered[2].second == annotated_haplotype[2].second + 1);
                
            }
            
            SECTION("A haplotype without lane numbers can be appended") {
                // Make a haplotype
                vector<handle_t> hap3;
                hap3.emplace_back(net_graph.get_handle(1, false));
                hap3.emplace_back(net_graph.get_handle(2, false));
                hap3.emplace_back(net_graph.get_handle(8, false));
                
                // Put it in the state
                auto added = state.append(hap3);
                
                SECTION("The state now has 3 haplotypes") {
                    REQUIRE(state.size() == 3);
                }
                
                SECTION("The returned annotated haplotype is correct") {
                    REQUIRE(added.size() == 3);
                    REQUIRE(added[0].first == hap3[0]);
                    REQUIRE(added[0].second == 2);
                    REQUIRE(added[1].first == hap3[1]);
                    REQUIRE(added[1].second == 1);
                    REQUIRE(added[2].first == hap3[2]);
                    REQUIRE(added[2].second == 2);
                }
                
                SECTION("The new haplotype can be traced back again") {
                    vector<pair<handle_t, size_t>> recovered;
                    
                    state.trace(2, false, [&](const handle_t& visit, size_t local_lane) {
                        recovered.emplace_back(visit, local_lane);
                    });
                    
                    REQUIRE(recovered == added);
                }
                
                SECTION("It can be deleted again") {
                    state.erase(2);
                    
                    REQUIRE(state.size() == 2);
                        
                    SECTION("The second haplotype can be traced back again") {
                        vector<pair<handle_t, size_t>> recovered;
                        
                        state.trace(0, false, [&](const handle_t& visit, size_t local_lane) {
                            recovered.emplace_back(visit, local_lane);
                        });
                        
                        REQUIRE(recovered == hap2);
                    }
                    
                    SECTION("The first haplotype can be traced back again") {
                        vector<pair<handle_t, size_t>> recovered;
                        
                        state.trace(1, false, [&](const handle_t& visit, size_t local_lane) {
                            recovered.emplace_back(visit, local_lane);
                        });
                        
                        REQUIRE(recovered.size() == 3);
                        REQUIRE(recovered[0].first == annotated_haplotype[0].first);
                        REQUIRE(recovered[0].second == annotated_haplotype[0].second + 1);
                        // The second mapping should not get bumped up.
                        REQUIRE(recovered[1].first == annotated_haplotype[1].first);
                        REQUIRE(recovered[1].second == annotated_haplotype[1].second);
                        REQUIRE(recovered[2].first == annotated_haplotype[2].first);
                        REQUIRE(recovered[2].second == annotated_haplotype[2].second + 1);
                        
                    }
                    
                }
                
                SECTION("It can be swapped with another haplotype") {
                    state.swap(0, 2);
                    
                    SECTION("Swapping haplotypes dows not change the overall size") {
                        REQUIRE(state.size() == 3);
                    }
                    
                    SECTION("The second haplotype added is now in overall lane 2") {
                        vector<pair<handle_t, size_t>> recovered;
                        
                        
                        state.trace(2, false, [&](const handle_t& visit, size_t local_lane) {
                            recovered.emplace_back(visit, local_lane);
                        });
                        
                        
                        
                        REQUIRE(recovered.size() == 2);
                        REQUIRE(recovered[0].first == hap2[0].first);
                        REQUIRE(recovered[0].second == 2);
                        REQUIRE(recovered[1].first == hap2[1].first);
                        REQUIRE(recovered[1].second == 2);
                    }
                    
                    SECTION("The third haplotype added is now in overall lane 0") {
                        vector<pair<handle_t, size_t>> recovered;
                        
                        state.trace(0, false, [&](const handle_t& visit, size_t local_lane) {
                            recovered.emplace_back(visit, local_lane);
                        });
                        
                        REQUIRE(recovered.size() == 3);
                        REQUIRE(recovered[0].first == hap3[0]);
                        REQUIRE(recovered[0].second == 0);
                        SECTION("Swapping haplotypes does not change interior child snarl lane assignments") {
                            // The second mapping should stay in place in its assigned lane
                            REQUIRE(recovered[1].first == hap3[1]);
                            REQUIRE(recovered[1].second == 1);
                        }
                        REQUIRE(recovered[2].first == hap3[2]);
                        REQUIRE(recovered[2].second == 0);
                    }
                    
                    SECTION("The first haplotype added is unaffected in overall lane 1") {
                        vector<pair<handle_t, size_t>> recovered;
                        
                        state.trace(1, false, [&](const handle_t& visit, size_t local_lane) {
                            recovered.emplace_back(visit, local_lane);
                        });
                        
                        REQUIRE(recovered.size() == 3);
                        REQUIRE(recovered[0].first == annotated_haplotype[0].first);
                        REQUIRE(recovered[0].second == annotated_haplotype[0].second + 1);
                        // The second mapping should not get bumped up.
                        REQUIRE(recovered[1].first == annotated_haplotype[1].first);
                        REQUIRE(recovered[1].second == annotated_haplotype[1].second);
                        REQUIRE(recovered[2].first == annotated_haplotype[2].first);
                        REQUIRE(recovered[2].second == annotated_haplotype[2].second + 1);
                        
                    }
                    
                    
                }
                
            }
            
            SECTION("A haplotype without lane numbers can be inserted") {
                // Make a haplotype
                vector<handle_t> hap3;
                hap3.emplace_back(net_graph.get_handle(1, false));
                hap3.emplace_back(net_graph.get_handle(2, false));
                hap3.emplace_back(net_graph.get_handle(8, false));
                
                // Put it in the state, at lane 1
                auto added = state.insert(1, hap3);
                
                SECTION("The returned annotated haplotype is correct") {
                    REQUIRE(added.size() == 3);
                    REQUIRE(added[0].first == hap3[0]);
                    REQUIRE(added[0].second == 1);
                    REQUIRE(added[1].first == hap3[1]);
                    // We don't actually care about our middle node lane assignment.
                    // It could be before or after other haplotypes; they're allowed to cross over each other arbitrarily.
                    REQUIRE(added[1].second >= 0);
                    REQUIRE(added[1].second <= 1);
                    REQUIRE(added[2].first == hap3[2]);
                    REQUIRE(added[2].second == 1);
                }
                
                SECTION("The new haplotype can be traced back again") {
                    vector<pair<handle_t, size_t>> recovered;
                    
                    state.trace(1, false, [&](const handle_t& visit, size_t local_lane) {
                        recovered.emplace_back(visit, local_lane);
                    });
                    
                    REQUIRE(recovered == added);
                }
                
                SECTION("The bumped-up haplotype can be traced back again") {
                    vector<pair<handle_t, size_t>> recovered;
                    
                    state.trace(2, false, [&](const handle_t& visit, size_t local_lane) {
                        recovered.emplace_back(visit, local_lane);
                    });
                    
                    REQUIRE(recovered.size() == 3);
                    REQUIRE(net_graph.get_id(recovered[0].first) == 1);
                    REQUIRE(recovered[0].second == 2);
                    REQUIRE(net_graph.get_id(recovered[1].first) == 2);
                    // Lane assignment at the middle visit may or may not have been pushed up.
                    REQUIRE(recovered[1].second >= 0);
                    REQUIRE(recovered[1].second <= 1);
                    REQUIRE(recovered[1].second != added[1].second);
                    REQUIRE(net_graph.get_id(recovered[2].first) == 8);
                    REQUIRE(recovered[2].second == 2);
                }
                
            }
        
        }
        
    }
    
    SECTION("Haplotypes cannot be added in reverse") {
        // Make a haplotype
        vector<pair<handle_t, size_t>> annotated_haplotype;
        
        // Say we go 8 rev, 2 rev (which is a child snarl), 1 rev
        annotated_haplotype.emplace_back(net_graph.get_handle(8, true), 0);
        annotated_haplotype.emplace_back(net_graph.get_handle(2, true), 0);
        annotated_haplotype.emplace_back(net_graph.get_handle(1, true), 0);
        
        // Try and fail to put it in the state
        REQUIRE_THROWS(state.insert(annotated_haplotype));
        
    }
    
    

}

TEST_CASE("GenomeState can hold and manipulate haplotypes", "[genomestate]") {
    

    // This graph will have a snarl from 1 to 8, a snarl from 2 to 7,
    // and a snarl from 3 to 5, all nested in each other.
    VG graph;
        
    Node* n1 = graph.create_node("GCA");
    Node* n2 = graph.create_node("T");
    Node* n3 = graph.create_node("G");
    Node* n4 = graph.create_node("CTGA");
    Node* n5 = graph.create_node("GCA");
    Node* n6 = graph.create_node("T");
    Node* n7 = graph.create_node("G");
    Node* n8 = graph.create_node("CTGA");
    
    Edge* e1 = graph.create_edge(n1, n2);
    Edge* e2 = graph.create_edge(n1, n8);
    Edge* e3 = graph.create_edge(n2, n3);
    Edge* e4 = graph.create_edge(n2, n6);
    Edge* e5 = graph.create_edge(n3, n4);
    Edge* e6 = graph.create_edge(n3, n5);
    Edge* e7 = graph.create_edge(n4, n5);
    Edge* e8 = graph.create_edge(n5, n7);
    Edge* e9 = graph.create_edge(n6, n7);
    Edge* e10 = graph.create_edge(n7, n8);
    
    // Work out its snarls
    CactusSnarlFinder bubble_finder(graph);
    SnarlManager snarl_manager = bubble_finder.find_snarls();

    // Get the top snarl
    const Snarl* top_snarl = snarl_manager.top_level_snarls().at(0);
    
    // Make sure it's what we expect.
    REQUIRE(top_snarl->start().node_id() == 1);
    REQUIRE(top_snarl->end().node_id() == 8);
    
    // Get the middle snarl
    const Snarl* middle_snarl = snarl_manager.children_of(top_snarl).at(0);
    
    // And the bottom snarl
    const Snarl* bottom_snarl = snarl_manager.children_of(middle_snarl).at(0);
    
    // Define the chromosome by telomere snarls (first and last)
    auto chromosome = make_pair(top_snarl, top_snarl);
    
    // Make a genome state for this genome, with the only top snarl being the
    // telomere snarls for the main chromosome
    GenomeState state(snarl_manager, &graph, {chromosome});
    
    // Make sure to get all the net graphs
    const NetGraph* top_graph = state.get_net_graph(top_snarl);
    const NetGraph* middle_graph = state.get_net_graph(middle_snarl);
    const NetGraph* bottom_graph = state.get_net_graph(bottom_snarl);
    
    SECTION("NetGraphs for snarls can be obtained") {
        REQUIRE(top_graph != nullptr);
        REQUIRE(middle_graph != nullptr);
        REQUIRE(bottom_graph != nullptr);
    }
    
    SECTION("GenomeState starts empty") {
        REQUIRE(state.count_haplotypes(chromosome) == 0);
    }
    
    SECTION("A haplotype can be added") {
        // Define a haplotype across the entire graph, in three levels
        InsertHaplotypeCommand insert;
        
        // We fill these with handles from the appropriate net graphs.
        // TODO: really we could just use the backing graph. Would that be better???
        
        // For the top snarl we go 1, 2 (child), and 8
        insert.insertions.emplace(top_snarl, vector<vector<pair<handle_t, size_t>>>{{
            {top_graph->get_handle(1, false), 0},
            {top_graph->get_handle(2, false), 0},
            {top_graph->get_handle(8, false), 0}
        }});
        
        // For the middle snarl we go 2, 3 (child), 7
        insert.insertions.emplace(middle_snarl, vector<vector<pair<handle_t, size_t>>>{{
            {middle_graph->get_handle(2, false), 0},
            {middle_graph->get_handle(3, false), 0},
            {middle_graph->get_handle(7, false), 0}
        }});
        
        // For the bottom snarl we go 3, 5 (skipping over 4)
        insert.insertions.emplace(bottom_snarl, vector<vector<pair<handle_t, size_t>>>{{
            {bottom_graph->get_handle(3, false), 0},
            {bottom_graph->get_handle(5, false), 0}
        }});
        
        // Execute the command and get the undo command
        GenomeStateCommand* undo = state.execute(&insert);
        
        SECTION("The added haplotype is counted") {
            REQUIRE(state.count_haplotypes(chromosome) == 1);
        }
        
        // Free the undo command
        delete undo;
    }
    
}
    
        
}
}

















