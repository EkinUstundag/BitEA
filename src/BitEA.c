#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "BitEA.h"
#include "stdgraph.h"


// Global array holding each vertex's degree (edge count).
// Size is graph_size, allocated/filled at the start of BitEA()
// and freed before it returns.
// Global arrays holding vertex data
int *degrees = NULL;
int *sorted_by_weight = NULL;

// Descending sort comparator
int comp_weight_desc(const void* a, const void* b, void* weights) {
    return (((int*)weights)[*(int*)b]) - (((int*)weights)[*(int*)a]);
}

int BitEA(
    int graph_size, 
    const block_t *edges, 
    int *weights, 
    int population_size,
    int base_color_count, 
    int max_gen_num, 
    block_t *best_solution, 
    int *best_fitness, 
    float *best_solution_time,
    int *uncolored_num
) {
    // Fill the global degree array
    degrees = malloc(graph_size * sizeof(int));
    count_edges(graph_size, edges, degrees);

    // Fill the global sorted weights array
    sorted_by_weight = malloc(graph_size * sizeof(int));
    for(int i = 0; i < graph_size; i++) sorted_by_weight[i] = i;
    qsort_r(sorted_by_weight, graph_size, sizeof(int), comp_weight_desc, (void*)weights);

    // Create the random population.
    block_t *population[population_size];
    int color_count[population_size];
    int uncolored[population_size];
    int fitness[population_size];
    
    for (int i = 0; i < population_size; i++) {
        population[i] = calloc(base_color_count * TOTAL_BLOCK_NUM((size_t)graph_size), sizeof(block_t));
        uncolored[i] = base_color_count;
        color_count[i] = base_color_count;
        // Leave initial fitness at __INT_MAX__. The greedy random starts often have conflicts.
        // We want the EA to immediately replace them with valid children from crossover.
        fitness[i] = __INT_MAX__; 
    }

    pop_complex_random(
        graph_size, edges, weights,
        population_size, population, base_color_count
    );

    struct timeval t1, t2;
    *best_solution_time = 0;
    gettimeofday(&t1, NULL);

    block_t *child = malloc(base_color_count * TOTAL_BLOCK_NUM(graph_size) * sizeof(block_t));
    
    int best_i = 0;
    int target_color = base_color_count; 
    int temp_uncolored;
    int parent1, parent2, child_colors, temp_fitness;
    int bad_parent;
    
    for(int i = 0; i < max_gen_num; i++) {
        memset(child, 0, (TOTAL_BLOCK_NUM(graph_size))*base_color_count*sizeof(block_t));

        // Pick 2 random parents
        parent1 = rand()%population_size;
        do { parent2 = rand()%population_size; } while (parent2 != parent1);

        // Do a crossover
        temp_fitness = crossover (
            graph_size, edges, weights,
            color_count[parent1], color_count[parent2], 
            population[parent1], population[parent2], 
            target_color, child, &child_colors, &temp_uncolored
        );

        // THE FIX: Massive Penalty for Conflicts
        // If crossover had to randomly allocate vertices (temp_uncolored > 0), it created conflicts.
        // We add a massive penalty so the EA aggressively filters out invalid solutions.
        int effective_fitness = temp_fitness;
        if (temp_uncolored > 0) {
            effective_fitness += (temp_uncolored * 1000000);
        }

        // Choose the worse parent based purely on effective fitness
        bad_parent = (fitness[parent1] > fitness[parent2]) ? parent1 : parent2;

        // Replace if the child has a better or equal penalized cost
        if(effective_fitness <= fitness[bad_parent]) {
            memmove(population[bad_parent], child, (TOTAL_BLOCK_NUM(graph_size))*base_color_count*sizeof(block_t));
            color_count[bad_parent] = child_colors;
            fitness[bad_parent] = effective_fitness;
            uncolored[bad_parent] = temp_uncolored;

            // Track global best (first valid child will easily beat __INT_MAX__)
            if (effective_fitness < fitness[best_i] || fitness[best_i] == __INT_MAX__) {
                best_i = bad_parent;
                gettimeofday(&t2, NULL);
                *best_solution_time = (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) / 1000000.0;   // us to ms
            }
        }
    }

    // Return the actual raw cost (removing the penalty for reporting purposes)
    *best_fitness = fitness[best_i] >= 1000000 ? fitness[best_i] - (uncolored[best_i] * 1000000) : fitness[best_i];
    *uncolored_num = uncolored[best_i];
    memcpy(best_solution, population[best_i], base_color_count * (TOTAL_BLOCK_NUM(graph_size)) * sizeof(block_t));

    // Free allocated space.
    free(child);
    for(int i = 0; i < population_size; i++)
        free(population[i]);
        
    free(degrees);
    degrees = NULL;
    free(sorted_by_weight);
    sorted_by_weight = NULL;

    return color_count[best_i];
}

int get_rand_color(int max_color_num, int colors_used, block_t used_color_list[]) {
    // There are no available colors.
    if(colors_used >= max_color_num) {
        return -1;

    // There are only 2 colors available, search for them linearly.
    } else if(colors_used > max_color_num - 2) {
        for(int i = 0; i < max_color_num; i++) {
            if(!(used_color_list[BLOCK_INDEX(i)] & MASK(i))) {
                used_color_list[BLOCK_INDEX(i)] |= MASK(i);
                return i;
            }
        }
    }

    // Randomly try to select an available color.
    int temp;
    while(1) {
        temp = rand()%max_color_num;
        if(!(used_color_list[BLOCK_INDEX(temp)] & MASK(temp))) {
            used_color_list[BLOCK_INDEX(temp)] |= MASK(temp);
            return temp;
        }
    }
}
// not used
int condition1(int conflict_worst, int weight_worst, int conflict_i, int weight_i){

    int mult_worst = conflict_worst * weight_worst;
    int mult_i = conflict_i * weight_i;

    return (mult_worst < mult_i ||
                    (mult_worst == mult_i &&
                     (conflict_worst < conflict_i || 
                     (conflict_worst == conflict_i && (rand()%2) ) )) ) ;
}

int condition2(int conflict_worst, int weight_worst, int conflict_i, int weight_i){

    int mult_worst = conflict_worst * weight_worst;
    int mult_i = conflict_i * weight_i;

    return (mult_worst > mult_i ||
                    (mult_worst == mult_i &&
                     (conflict_worst < conflict_i || 
                     (conflict_worst == conflict_i && (rand()%2) ) )) ) ;
}

int condition3(int conflict_worst, int weight_worst, int conflict_i, int weight_i){
    
    double divide_worst = conflict_worst / (double) weight_worst;
    double divide_i = conflict_i / (double) weight_i;
    
    return (divide_worst < divide_i ||
                    (divide_worst == divide_i &&
                     (conflict_worst < conflict_i || 
                     (conflict_worst == conflict_i && (rand()%2) ) )) ) ;
}

//
int condition4(int conflict_worst, int weight_worst, int conflict_i, int weight_i){
        
    return (weight_worst < weight_i ||
                    (weight_worst == weight_i &&
                     (conflict_worst < conflict_i || 
                     (conflict_worst == conflict_i && (rand()%2) ) )) ) ;
}

int condition5(int conflict_worst, int weight_worst, int conflict_i, int weight_i){
    
    double divide_worst = weight_worst / (double) conflict_worst;
    double divide_i = weight_i / (double) conflict_i ;
    
    return (divide_worst > divide_i ||
                    (divide_worst == divide_i &&
                     (conflict_worst < conflict_i || 
                     (conflict_worst == conflict_i && (rand()%2) ) )) ) ;
}

int condition6(int conflict_worst, int weight_worst, int conflict_i, int weight_i){
    
    double divide_worst = weight_worst * weight_worst / (double) conflict_worst;
    double divide_i = weight_i * weight_i / (double) conflict_i ;
    
    return (divide_worst > divide_i ||
                    (divide_worst == divide_i &&
                     (conflict_worst < conflict_i || 
                     (conflict_worst == conflict_i && (rand()%2) ) )) ) ;
}

int condition7(int conflict_worst, int weight_worst,int degree_worst, int conflict_i, int weight_i,int degree_i){
    
    double divide_worst = weight_worst * weight_worst / (double)(conflict_worst * degree_worst) ;
    double divide_i = weight_i * weight_i / (double) (conflict_i * degree_i) ;
    
    return (divide_worst > divide_i ||
                    (divide_worst == divide_i &&
                     (conflict_worst < conflict_i || 
                     (conflict_worst == conflict_i && (rand()%2) ) )) ) ;
}
//  max weight * degree
int condition8(int conflict_worst, int weight_worst,int degree_worst, int conflict_i, int weight_i,int degree_i){
    
    int mult_worst = weight_worst * degree_worst;
    int mult_i = weight_i * degree_i;
    
    return (mult_worst < mult_i ||
                    (mult_worst == mult_i &&
                     (conflict_worst < conflict_i || 
                     (conflict_worst == conflict_i && (rand()%2) ) )) ) ;
}

int condition9(int conflict_worst,int degree_worst, int conflict_i,int degree_i){
    
    int mult_worst = conflict_worst * degree_worst;
    int mult_i = conflict_i * degree_i;
    
    return (mult_worst < mult_i ||
                    (mult_worst == mult_i &&
                     (conflict_worst < conflict_i || 
                     (conflict_worst == conflict_i && (rand()%2) ) )) ) ;
}
//  min weight * degree
int condition10(int conflict_worst, int weight_worst,int degree_worst, int conflict_i, int weight_i,int degree_i){
    
    int mult_worst = weight_worst * degree_worst;
    int mult_i = weight_i * degree_i;
    
    return (mult_worst > mult_i ||
                    (mult_worst == mult_i &&
                     (conflict_worst < conflict_i || 
                     (conflict_worst == conflict_i && (rand()%2) ) )) ) ;
}
//  min weight * degree * conflict
int condition11(int conflict_worst, int weight_worst,int degree_worst, int conflict_i, int weight_i,int degree_i){
    
    int mult_worst = weight_worst * degree_worst * conflict_worst;
    int mult_i = weight_i * degree_i * conflict_i;
    
    return (mult_worst > mult_i ||
                    (mult_worst == mult_i &&
                     (conflict_worst < conflict_i || 
                     (conflict_worst == conflict_i && (rand()%2) ) )) ) ;
}

/* vertices degree: vertice bağlı olduğu diğer vertice sayısı
degree array oluştur
condition7: minimum weight * weight / (degree * conflict)
condition8: maximum weight*degree
condition9: max conflict * degree
eşit olasılık
*/

void fix_conflicts(
    int graph_size,
    const block_t *edges, 
    const int *weights,
    int *conflict_count,
    int *total_conflicts,
    block_t *color,
    block_t *pool,
    int *pool_total,
    int decision_criteria // Kept to avoid changing the function signature in merge_and_fix
) {
    block_t (*edges_p)[][TOTAL_BLOCK_NUM(graph_size)] = (block_t (*)[][TOTAL_BLOCK_NUM(graph_size)])edges;
    
    int i, worst_vert, vert_block;
    block_t vert_mask;

    while(*total_conflicts > 0) {
        worst_vert = -1;

        // Find the most problematic vertex to remove
        for(i = 0; i < graph_size; i++) {
            // Only evaluate vertices currently in this color that actively contribute to conflicts
            if (CHECK_COLOR(color, i) && conflict_count[i] > 0) {
                if (worst_vert == -1) {
                    worst_vert = i;
                } else {
                    // Score formula: Conflicts resolved / Weight penalty incurred
                    // A higher score means it's a better candidate to be sent to the pool.
                    double weight_worst_safe = weights[worst_vert] > 0 ? weights[worst_vert] : 1;
                    double weight_i_safe = weights[i] > 0 ? weights[i] : 1;
                    
                    double score_worst = (double)conflict_count[worst_vert] / weight_worst_safe;
                    double score_i = (double)conflict_count[i] / weight_i_safe;

                    if (score_i > score_worst) {
                        worst_vert = i;
                    } else if (score_i == score_worst) {
                        // Tie-breaker 1: Remove the lighter vertex
                        if (weights[i] < weights[worst_vert]) {
                            worst_vert = i;
                        } 
                        // Tie-breaker 2: Remove vertex with lower overall degree (easier to color later)
                        else if (weights[i] == weights[worst_vert] && degrees[i] < degrees[worst_vert]) {
                            worst_vert = i;
                        }
                        // Tie-breaker 3: Random choice for stochastic diversity
                        else if (weights[i] == weights[worst_vert] && degrees[i] == degrees[worst_vert] && (rand() % 2)) {
                            worst_vert = i;
                        }
                    }
                }
            }
        }

        // Safety break to prevent infinite loops if total_conflicts gets out of sync
        if (worst_vert == -1) break; 

        // Update other conflict counters
        vert_mask = MASK(worst_vert);
        vert_block = BLOCK_INDEX(worst_vert);
        for(i = 0; i < graph_size; i++) {
            if(CHECK_COLOR(color, i) && ((*edges_p)[i][vert_block] & vert_mask)) {
                conflict_count[i]--;
            }
        }

        // Remove the chosen vertex from the color class and drop it in the pool
        color[vert_block] &= ~vert_mask;
        pool[vert_block] |= vert_mask;
        (*pool_total)++;

        // Update the total number of conflicts
        (*total_conflicts) -= conflict_count[worst_vert];
        conflict_count[worst_vert] = 0;
    }
}

void merge_and_fix(
    int graph_size,
    const block_t *edges, 
    const int *weights,
    const block_t **parent_color,
    block_t *child_color,
    block_t *pool,
    int *pool_count,
    block_t *used_vertex_list,
    int *used_vertex_count
) {
    // Merge the two colors
    int temp_v_count = 0;  
    if(parent_color[0] != NULL && parent_color[1] != NULL)
        for(int i = 0; i < (TOTAL_BLOCK_NUM(graph_size)); i++) {
            child_color[i] = ((parent_color[0][i] | parent_color[1][i]) & ~(used_vertex_list[i]));
            temp_v_count += popcountl(child_color[i]);
        }

    else if(parent_color[0] != NULL)
        for(int i = 0; i < (TOTAL_BLOCK_NUM(graph_size)); i++) {
            child_color[i] = (parent_color[0][i] & ~(used_vertex_list[i]));
            temp_v_count += popcountl(child_color[i]);
        }

    else if(parent_color[1] != NULL)
        for(int i = 0; i < (TOTAL_BLOCK_NUM(graph_size)); i++) {
            child_color[i] = (parent_color[1][i] & ~(used_vertex_list[i]));
            temp_v_count += popcountl(child_color[i]);
        }

    (*used_vertex_count) += temp_v_count;

    // Merge the pool with the new color
    for(int i = 0; i < (TOTAL_BLOCK_NUM(graph_size)); i++) {
        child_color[i] |= pool[i];
        used_vertex_list[i] |= child_color[i];
    }

    memset(pool, 0, (TOTAL_BLOCK_NUM(graph_size))*sizeof(block_t));
    (*pool_count) = 0;


    // List of conflict count per vertex.
    int conflict_count[graph_size];
    memset(conflict_count, 0, graph_size*sizeof(int));

    // Count conflicts.
    int total_conflicts = count_conflicts(
        graph_size,
        child_color,
        edges,
        conflict_count
    );

    //
    int decision_criteria = rand();

    // Fix the conflicts.
    fix_conflicts(
        graph_size,
        edges,
        weights,
        conflict_count,
        &total_conflicts,
        child_color,
        pool,
        pool_count,
        decision_criteria
    );
}

void search_back(
    int graph_size,
    const block_t *edges, 
    const int *weights,
    block_t *child, 
    int color_count,
    block_t *pool,
    int *pool_count
) {
    block_t (*edges_p)[][TOTAL_BLOCK_NUM(graph_size)] = (block_t (*)[][TOTAL_BLOCK_NUM(graph_size)])edges;
    block_t (*child_p)[][TOTAL_BLOCK_NUM(graph_size)] = (block_t (*)[][TOTAL_BLOCK_NUM(graph_size)])child;

    int conflict_count, last_conflict, last_conflict_block = 0;
    block_t i_mask, temp_mask, last_conflict_mask = 0;
    int i, j, k, i_block, v;

    // Calculate max weight for each color class to optimize WVCP insertions
    int color_max_weight[color_count];
    memset(color_max_weight, 0, color_count * sizeof(int));
    for(j = 0; j < color_count; j++) {
        for(k = 0; k < graph_size; k++) {
            if(CHECK_COLOR((*child_p)[j], k) && weights[k] > color_max_weight[j])
                color_max_weight[j] = weights[k];
        }
    }

    // Search back and try placing vertices from the pool. 
    // Loop over vertices in DECREASING order of weight!
    for(v = 0; v < graph_size && (*pool_count) > 0; v++) {
        i = sorted_by_weight != NULL ? sorted_by_weight[v] : v; 
        i_block = BLOCK_INDEX(i);
        i_mask = MASK(i);

        // Check if the vertex is in the pool.
        if(pool[i_block] & i_mask) {
            
            int best_color = -1;
            int best_cost_diff = __INT_MAX__;
            int best_conflict_count = -1;
            int best_last_conflict = -1;
            int best_last_conflict_block = -1;
            block_t best_last_conflict_mask = 0;

            // Loop through every previous color to find the cheapest insertion
            for(j = 0; j < color_count; j++) {
                conflict_count = 0;
                for(k = 0; k < TOTAL_BLOCK_NUM(graph_size); k++) {
                    temp_mask = (*child_p)[j][k] & (*edges_p)[i][k];
                    if(temp_mask) {
                        conflict_count += popcountl(temp_mask);
                        if(conflict_count > 1) break;
                        last_conflict = sizeof(block_t)*8*(k + 1) - 1 - __builtin_clzl(temp_mask);
                        last_conflict_mask = temp_mask;
                        last_conflict_block = k;
                    }
                }

                if(conflict_count == 0) {
                    // Cost increase is 0 if vertex is lighter than the class max
                    int cost_diff = weights[i] > color_max_weight[j] ? weights[i] - color_max_weight[j] : 0;
                    if (cost_diff < best_cost_diff || (cost_diff == best_cost_diff && best_conflict_count == 1)) {
                        best_cost_diff = cost_diff;
                        best_color = j;
                        best_conflict_count = 0;
                        if (cost_diff == 0) break; // Perfect match found, stop searching
                    }
                } 
                else if (conflict_count == 1 && weights[last_conflict] < weights[i]) {
                    int cost_diff = weights[i] > color_max_weight[j] ? weights[i] - color_max_weight[j] : 0;
                    if (cost_diff < best_cost_diff && best_conflict_count != 0) {
                        best_cost_diff = cost_diff;
                        best_color = j;
                        best_conflict_count = 1;
                        best_last_conflict = last_conflict;
                        best_last_conflict_block = last_conflict_block;
                        best_last_conflict_mask = last_conflict_mask;
                    }
                }
            }

            // Execute the best move
            if (best_color != -1) {
                if (best_conflict_count == 0) {
                    (*child_p)[best_color][i_block] |= i_mask;
                    pool[i_block] &= ~i_mask;
                    (*pool_count)--;
                    if (weights[i] > color_max_weight[best_color]) 
                        color_max_weight[best_color] = weights[i];
                } else if (best_conflict_count == 1) {
                    (*child_p)[best_color][i_block] |= i_mask;
                    pool[i_block] &= ~i_mask;
                    
                    (*child_p)[best_color][best_last_conflict_block] &= ~best_last_conflict_mask;
                    pool[best_last_conflict_block] |= best_last_conflict_mask;
                    if (weights[i] > color_max_weight[best_color]) 
                        color_max_weight[best_color] = weights[i];
                }
            }
        }
    }
}

void local_search(
    int graph_size,
    const block_t *edges, 
    const int *weights,
    block_t *child, 
    int color_count,
    block_t *pool,
    int *pool_count
) {
    block_t (*edges_p)[][TOTAL_BLOCK_NUM(graph_size)] = (block_t (*)[][TOTAL_BLOCK_NUM(graph_size)])edges;
    block_t (*child_p)[][TOTAL_BLOCK_NUM(graph_size)] = (block_t (*)[][TOTAL_BLOCK_NUM(graph_size)])child;

    int i, j, k, h, i_block, v;
    block_t i_mask, temp_mask;
    int competition;
    int conflict_count;
    block_t conflict_array[TOTAL_BLOCK_NUM(graph_size)];

    // Process vertices in DECREASING order of weight
    for(v = 0; v < graph_size && (*pool_count) > 0; v++) {
        i = sorted_by_weight != NULL ? sorted_by_weight[v] : v;
        i_block = BLOCK_INDEX(i);
        i_mask = MASK(i);

        if(pool[i_block] & i_mask) {
            for(j = 0; j < color_count; j++) {  
                conflict_count = 0;
                competition = 0;
                for(k = 0; k < TOTAL_BLOCK_NUM(graph_size); k++) {
                    conflict_array[k] = (*edges_p)[i][k] & (*child_p)[j][k];
                    if(conflict_array[k]) {
                        temp_mask = conflict_array[k];
                        conflict_count += popcountl(temp_mask);
                        for(h = 0; h < sizeof(block_t)*8; h++)
                            if((temp_mask >> h) & (block_t)1)
                                competition += weights[k*8*sizeof(block_t)+h];
                    }
                }

                if(competition == 0) {
                    (*child_p)[j][i_block] |= i_mask;
                    pool[i_block] &= ~i_mask;
                    (*pool_count) += conflict_count - 1;
                    break;
                } else if(competition < weights[i]) {
                    for(k = 0; k < TOTAL_BLOCK_NUM(graph_size); k++) {
                        (*child_p)[j][k] &= ~conflict_array[k];
                        pool[k] |= conflict_array[k];
                    }

                    (*child_p)[j][i_block] |= i_mask;
                    pool[i_block] &= ~i_mask;
                    (*pool_count) += conflict_count - 1;
                    break;
                }
            }
        }
    }
}

int crossover (
    int graph_size, 
    const block_t *edges, 
    const int *weights,
    int color_num1, 
    int color_num2, 
    const block_t *parent1, 
    const block_t *parent2, 
    int target_color_count,
    block_t *child,
    int *child_color_count,
    int *uncolored
) {
    const block_t (*parent1_p)[][TOTAL_BLOCK_NUM(graph_size)] = (block_t (*)[][TOTAL_BLOCK_NUM(graph_size)])parent1;
    const block_t (*parent2_p)[][TOTAL_BLOCK_NUM(graph_size)] = (block_t (*)[][TOTAL_BLOCK_NUM(graph_size)])parent2;
    block_t (*child_p)[][TOTAL_BLOCK_NUM(graph_size)] = (block_t (*)[][TOTAL_BLOCK_NUM(graph_size)])child;

    // max number of colors of the two parents.
    int max_color_num = color_num1 > color_num2 ? color_num1 : color_num2;

    // list of used colors in the parents.
    block_t used_color_list[2][TOTAL_BLOCK_NUM(max_color_num)];
    memset(used_color_list, 0, 2*TOTAL_BLOCK_NUM(max_color_num)*sizeof(block_t));

    // list of used vertices in the parents.
    block_t used_vertex_list[TOTAL_BLOCK_NUM(graph_size)];
    memset(used_vertex_list, 0, (TOTAL_BLOCK_NUM(graph_size))*sizeof(block_t));
    int used_vertex_count = 0;

    // Pool.
    block_t pool[TOTAL_BLOCK_NUM(graph_size)];
    memset(pool, 0, (TOTAL_BLOCK_NUM(graph_size))*sizeof(block_t));
    int pool_count = 0;


    int color1, color2, last_color = 0;
    int i, j;
    const block_t *chosen_parent_colors[2];
    for(i = 0; i < target_color_count; i++) {
        // The child still has vertices that weren't used.
        if(used_vertex_count < graph_size) {
            // Pick 2 random colors.
            color1 = get_rand_color(color_num1, i, used_color_list[0]);
            color2 = get_rand_color(color_num2, i, used_color_list[1]);
            chosen_parent_colors[0] = color1 == -1 ? NULL : (*parent1_p)[color1];
            chosen_parent_colors[1] = color2 == -1 ? NULL : (*parent2_p)[color2];

            merge_and_fix(
                graph_size,
                edges,
                weights,
                chosen_parent_colors,
                (*child_p)[i],
                pool,
                &pool_count,
                used_vertex_list,
                &used_vertex_count
            );

        // If all of the vertices were used and the pool is empty, exit the loop.
        } else if(pool_count == 0) {
            break;
        }

        search_back(
            graph_size,
            edges,
            weights,
            child, 
            i,
            pool,
            &pool_count
        );
    }

    // Record the last color.
    last_color = i;


    // If not all the vertices were visited, drop them in the pool.
    if(used_vertex_count < graph_size) {
        for(j = 0; j < (TOTAL_BLOCK_NUM(graph_size)); j++)
            pool[j] |= ~used_vertex_list[j];
        pool[TOTAL_BLOCK_NUM(graph_size) - 1] &= ((0xFFFFFFFFFFFFFFFF) >> (TOTAL_BLOCK_NUM(graph_size)*sizeof(block_t)*8 - graph_size));

        pool_count += (graph_size - used_vertex_count);
        used_vertex_count = graph_size;
        memset(used_vertex_list, 0xFF, (TOTAL_BLOCK_NUM(graph_size))*sizeof(block_t));
    }

    local_search(
        graph_size,
        edges,
        weights,
        child,
        target_color_count,
        pool,
        &pool_count
    );

// If the pool is not empty, randomly allocate the remaining vertices in the colors.
    int fitness = 0, temp_block;
    block_t temp_mask;
    if(pool_count > 0) {
        int color_num;
        for(i = 0; i < graph_size; i++) {
            temp_block = BLOCK_INDEX(i);
            temp_mask = MASK(i);
            if(pool[temp_block] & temp_mask) {
                color_num = rand()%target_color_count;
                (*child_p)[color_num][temp_block] |= temp_mask;

                if(color_num + 1 > last_color)
                    last_color = color_num + 1;
            }
        }
    }

    // Weighted Vertex Coloring Problem fitness: sum, over every color class
    // actually in use, of the heaviest vertex assigned to that class.
    int color_max_weight;
    for(i = 0; i < last_color; i++) {
        color_max_weight = 0;
        for(j = 0; j < graph_size; j++) {
            if(CHECK_COLOR((*child_p)[i], j) && weights[j] > color_max_weight)
                color_max_weight = weights[j];
        }
        fitness += color_max_weight;
    }

    *uncolored = pool_count;
    *child_color_count = last_color;
    return fitness;
}
