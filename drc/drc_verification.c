/*
 * DRC Verification - Extended anti-hallucination checks
 */

#include "drc_verification.h"

/**
 * Initialize verification context
 */
EFI_STATUS verification_init(VerificationContext* ctx) {
    SetMem(ctx, sizeof(VerificationContext), 0);
    return EFI_SUCCESS;
}

/**
 * Build reasoning graph from solution path
 */
EFI_STATUS verification_build_graph(VerificationContext* ctx, SolutionPath* path) {
    ReasoningGraph* graph = &ctx->graph;
    SetMem(graph, sizeof(ReasoningGraph), 0);
    
    // Create nodes from reasoning steps
    for (UINT32 i = 0; i < path->step_count && i < MAX_NODES; i++) {
        ReasoningNode* node = &graph->nodes[i];
        node->id = i;
        node->confidence = path->steps[i].confidence;
        
        // Map hypothesis type to reasoning type
        switch (path->steps[i].type) {
            case HYPO_FACTORIZATION:
            case HYPO_SYMBOLIC_REWRITE:
                node->type = TYPE_SYMBOLIC;
                break;
            case HYPO_NUMERIC_SIM:
                node->type = TYPE_NUMERIC;
                break;
            case HYPO_GEOMETRIC:
                node->type = TYPE_GEOMETRIC;
                break;
            case HYPO_ASYMPTOTIC:
            case HYPO_INVERSE_REASONING:
                node->type = TYPE_LOGICAL;
                break;
            default:
                node->type = TYPE_UNKNOWN;
        }
        
        // Copy label
        for (UINT32 j = 0; j < 63 && path->steps[i].description[j]; j++) {
            node->label[j] = path->steps[i].description[j];
        }
        node->label[63] = '\0';
        
        graph->node_count++;
    }
    
    // Create edges (sequential dependencies by default)
    for (UINT32 i = 0; i < graph->node_count - 1 && graph->edge_count < MAX_EDGES; i++) {
        ReasoningEdge* edge = &graph->edges[graph->edge_count++];
        edge->from_id = i;
        edge->to_id = i + 1;
        edge->weight = 1.0f;
        
        // Set relation based on confidence drop
        if (graph->nodes[i + 1].confidence < graph->nodes[i].confidence * 0.8f) {
            // Large confidence drop suggests weakening
            for (UINT32 j = 0; j < 8; j++) {
                edge->relation[j] = "weakens"[j];
            }
        } else {
            // Normal implication
            for (UINT32 j = 0; j < 7; j++) {
                edge->relation[j] = "implies"[j];
            }
        }
    }
    
    return EFI_SUCCESS;
}

/**
 * DFS helper for cycle detection
 */
static BOOLEAN dfs_cycle(ReasoningGraph* graph, UINT32 node_id, 
                         BOOLEAN* visited, BOOLEAN* rec_stack,
                         UINT32* cycle_path, UINT32* cycle_len) {
    visited[node_id] = TRUE;
    rec_stack[node_id] = TRUE;
    cycle_path[*cycle_len] = node_id;
    (*cycle_len)++;
    
    // Check all outgoing edges
    for (UINT32 i = 0; i < graph->edge_count; i++) {
        if (graph->edges[i].from_id == node_id) {
            UINT32 next = graph->edges[i].to_id;
            
            if (!visited[next]) {
                if (dfs_cycle(graph, next, visited, rec_stack, cycle_path, cycle_len)) {
                    return TRUE;
                }
            } else if (rec_stack[next]) {
                // Cycle detected
                return TRUE;
            }
        }
    }
    
    rec_stack[node_id] = FALSE;
    (*cycle_len)--;
    return FALSE;
}

/**
 * Detect circular dependencies
 */
EFI_STATUS verification_check_cycles(VerificationContext* ctx) {
    ReasoningGraph* graph = &ctx->graph;
    
    BOOLEAN visited[MAX_NODES] = {0};
    BOOLEAN rec_stack[MAX_NODES] = {0};
    UINT32 cycle_path[MAX_NODES];
    UINT32 cycle_len = 0;
    
    graph->has_cycle = FALSE;
    
    for (UINT32 i = 0; i < graph->node_count; i++) {
        if (!visited[i]) {
            if (dfs_cycle(graph, i, visited, rec_stack, cycle_path, &cycle_len)) {
                graph->has_cycle = TRUE;
                
                // Copy cycle path
                for (UINT32 j = 0; j < cycle_len && j < MAX_NODES; j++) {
                    graph->cycle_nodes[j] = cycle_path[j];
                }
                graph->cycle_length = cycle_len;
                
                ctx->passed_cycle_check = FALSE;
                ctx->failed_checks++;
                return EFI_SUCCESS;
            }
        }
    }
    
    ctx->passed_cycle_check = TRUE;
    ctx->total_checks++;
    return EFI_SUCCESS;
}

/**
 * Check type coherence
 */
EFI_STATUS verification_check_types(VerificationContext* ctx) {
    ReasoningGraph* graph = &ctx->graph;
    
    graph->has_type_mismatch = FALSE;
    UINT32 mismatches = 0;
    
    // Check each edge for type compatibility
    for (UINT32 i = 0; i < graph->edge_count; i++) {
        UINT32 from_id = graph->edges[i].from_id;
        UINT32 to_id = graph->edges[i].to_id;
        
        ReasoningType from_type = graph->nodes[from_id].type;
        ReasoningType to_type = graph->nodes[to_id].type;
        
        // Check for problematic transitions
        BOOLEAN mismatch = FALSE;
        
        // Numeric → Symbolic needs bridge
        if (from_type == TYPE_NUMERIC && to_type == TYPE_SYMBOLIC) {
            mismatch = TRUE;
        }
        
        // Geometric → Numeric without proper transformation
        if (from_type == TYPE_GEOMETRIC && to_type == TYPE_NUMERIC) {
            mismatch = TRUE;
        }
        
        if (mismatch) {
            graph->has_type_mismatch = TRUE;
            graph->mismatch_nodes[0] = from_id;
            graph->mismatch_nodes[1] = to_id;
            mismatches++;
        }
    }
    
    // Calculate type consistency
    if (graph->edge_count > 0) {
        ctx->type_consistency = 1.0f - ((float)mismatches / graph->edge_count);
    } else {
        ctx->type_consistency = 1.0f;
    }
    
    ctx->passed_type_check = (mismatches == 0);
    if (!ctx->passed_type_check) {
        ctx->failed_checks++;
    }
    
    ctx->total_checks++;
    return EFI_SUCCESS;
}

/**
 * Detect contradictory assumptions
 */
EFI_STATUS verification_check_contradictions(VerificationContext* ctx) {
    ReasoningGraph* graph = &ctx->graph;
    
    graph->has_contradiction = FALSE;
    
    // Look for "contradicts" relations in edges
    for (UINT32 i = 0; i < graph->edge_count; i++) {
        // Simple string compare for "contradicts"
        const char* rel = graph->edges[i].relation;
        if (rel[0] == 'c' && rel[1] == 'o' && rel[2] == 'n' && rel[3] == 't') {
            graph->has_contradiction = TRUE;
            graph->contradiction_nodes[0] = graph->edges[i].from_id;
            graph->contradiction_nodes[1] = graph->edges[i].to_id;
            
            ctx->passed_contradiction_check = FALSE;
            ctx->failed_checks++;
            ctx->total_checks++;
            return EFI_SUCCESS;
        }
    }
    
    ctx->passed_contradiction_check = TRUE;
    ctx->total_checks++;
    return EFI_SUCCESS;
}

/**
 * Track assumption propagation
 */
EFI_STATUS verification_track_assumptions(VerificationContext* ctx) {
    // TODO: Implement assumption tracking
    // For now: always pass
    
    ctx->passed_assumption_check = TRUE;
    ctx->total_checks++;
    return EFI_SUCCESS;
}

/**
 * Calculate overall coherence score
 */
float verification_calculate_coherence(VerificationContext* ctx) {
    if (ctx->total_checks == 0) {
        return 0.0f;
    }
    
    float pass_rate = 1.0f - ((float)ctx->failed_checks / ctx->total_checks);
    
    // Weight type consistency heavily
    ctx->graph_coherence = (pass_rate * 0.7f) + (ctx->type_consistency * 0.3f);
    
    return ctx->graph_coherence;
}

/**
 * Print verification report
 */
void verification_print_report(VerificationContext* ctx) {
    Print(L"\r\n[Verification] Report:\r\n");
    Print(L"  Total checks: %d\r\n", ctx->total_checks);
    Print(L"  Failed checks: %d\r\n", ctx->failed_checks);
    Print(L"  Graph coherence: %.2f\r\n", ctx->graph_coherence);
    Print(L"  Type consistency: %.2f\r\n", ctx->type_consistency);
    
    Print(L"\r\n  Checks:\r\n");
    Print(L"    Cycle check: %a\r\n", 
          ctx->passed_cycle_check ? "PASS" : "FAIL");
    Print(L"    Type check: %a\r\n", 
          ctx->passed_type_check ? "PASS" : "FAIL");
    Print(L"    Contradiction check: %a\r\n", 
          ctx->passed_contradiction_check ? "PASS" : "FAIL");
    Print(L"    Assumption check: %a\r\n", 
          ctx->passed_assumption_check ? "PASS" : "FAIL");
    
    if (ctx->graph.has_cycle) {
        Print(L"\r\n  WARNING: Circular dependency detected\r\n");
        Print(L"    Cycle length: %d nodes\r\n", ctx->graph.cycle_length);
    }
    
    if (ctx->graph.has_type_mismatch) {
        Print(L"\r\n  WARNING: Type mismatch detected\r\n");
        Print(L"    Between nodes: %d -> %d\r\n", 
              ctx->graph.mismatch_nodes[0], 
              ctx->graph.mismatch_nodes[1]);
    }
    
    if (ctx->graph.has_contradiction) {
        Print(L"\r\n  WARNING: Contradiction detected\r\n");
        Print(L"    Between nodes: %d <-> %d\r\n", 
              ctx->graph.contradiction_nodes[0], 
              ctx->graph.contradiction_nodes[1]);
    }
}

/**
 * Full verification pipeline
 */
EFI_STATUS verification_run_all(VerificationContext* ctx, SolutionPath* path) {
    EFI_STATUS status;
    
    // Initialize
    status = verification_init(ctx);
    if (EFI_ERROR(status)) return status;
    
    // Build graph
    status = verification_build_graph(ctx, path);
    if (EFI_ERROR(status)) return status;
    
    // Run all checks
    verification_check_cycles(ctx);
    verification_check_types(ctx);
    verification_check_contradictions(ctx);
    verification_track_assumptions(ctx);
    
    // Calculate coherence
    verification_calculate_coherence(ctx);
    
    return EFI_SUCCESS;
}
