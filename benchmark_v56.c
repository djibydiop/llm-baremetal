// Benchmark v5.6 optimizations
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

// v5.5 matmul (4x unroll)
void matmul_v55(float* xout, float* x, float* w, int n, int d) {
    for (int i = 0; i < d; i++) {
        float val = 0.0f;
        int j = 0;
        for (; j < n - 3; j += 4) {
            val += w[i * n + j + 0] * x[j + 0];
            val += w[i * n + j + 1] * x[j + 1];
            val += w[i * n + j + 2] * x[j + 2];
            val += w[i * n + j + 3] * x[j + 3];
        }
        for (; j < n; j++) {
            val += w[i * n + j] * x[j];
        }
        xout[i] = val;
    }
}

// v5.6 matmul (8x unroll + 4 accumulators)
void matmul_v56(float* xout, float* x, float* w, int n, int d) {
    for (int i = 0; i < d; i++) {
        float val0 = 0.0f, val1 = 0.0f, val2 = 0.0f, val3 = 0.0f;
        int j = 0;
        for (; j < n - 7; j += 8) {
            const float* wrow = &w[i * n + j];
            const float* xptr = &x[j];
            val0 += wrow[0] * xptr[0];
            val1 += wrow[1] * xptr[1];
            val2 += wrow[2] * xptr[2];
            val3 += wrow[3] * xptr[3];
            val0 += wrow[4] * xptr[4];
            val1 += wrow[5] * xptr[5];
            val2 += wrow[6] * xptr[6];
            val3 += wrow[7] * xptr[7];
        }
        float val = val0 + val1 + val2 + val3;
        for (; j < n; j++) {
            val += w[i * n + j] * x[j];
        }
        xout[i] = val;
    }
}

// v5.5 rmsnorm (naive)
void rmsnorm_v55(float* o, float* x, float* weight, int size) {
    float ss = 0.0f;
    for (int j = 0; j < size; j++) {
        ss += x[j] * x[j];
    }
    ss /= size;
    ss += 1e-5f;
    ss = 1.0f / sqrtf(ss);
    for (int j = 0; j < size; j++) {
        o[j] = weight[j] * (ss * x[j]);
    }
}

// v5.6 rmsnorm (4x unroll)
void rmsnorm_v56(float* o, float* x, float* weight, int size) {
    float ss0 = 0.0f, ss1 = 0.0f, ss2 = 0.0f, ss3 = 0.0f;
    int j = 0;
    for (; j < size - 3; j += 4) {
        ss0 += x[j+0] * x[j+0];
        ss1 += x[j+1] * x[j+1];
        ss2 += x[j+2] * x[j+2];
        ss3 += x[j+3] * x[j+3];
    }
    float ss = ss0 + ss1 + ss2 + ss3;
    for (; j < size; j++) {
        ss += x[j] * x[j];
    }
    ss /= size;
    ss += 1e-5f;
    ss = 1.0f / sqrtf(ss);
    j = 0;
    for (; j < size - 3; j += 4) {
        o[j+0] = weight[j+0] * (ss * x[j+0]);
        o[j+1] = weight[j+1] * (ss * x[j+1]);
        o[j+2] = weight[j+2] * (ss * x[j+2]);
        o[j+3] = weight[j+3] * (ss * x[j+3]);
    }
    for (; j < size; j++) {
        o[j] = weight[j] * (ss * x[j]);
    }
}

int main() {
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║  LLM Bare-Metal v5.6 Performance Benchmark    ║\n");
    printf("╚════════════════════════════════════════════════╝\n\n");
    
    // Test dimensions (similar to Stories110M)
    int dim = 768;
    int hidden_dim = 2048;
    int iterations = 1000;
    
    // Allocate test data
    float* x = (float*)malloc(dim * sizeof(float));
    float* w = (float*)malloc(dim * hidden_dim * sizeof(float));
    float* out = (float*)malloc(hidden_dim * sizeof(float));
    float* weight = (float*)malloc(dim * sizeof(float));
    
    // Initialize with random data
    srand(42);
    for (int i = 0; i < dim; i++) {
        x[i] = (float)rand() / RAND_MAX;
        weight[i] = (float)rand() / RAND_MAX;
    }
    for (int i = 0; i < dim * hidden_dim; i++) {
        w[i] = (float)rand() / RAND_MAX;
    }
    
    // Benchmark MatMul v5.5
    printf("🔄 Testing MatMul v5.5 (4x unroll)...\n");
    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        matmul_v55(out, x, w, dim, hidden_dim);
    }
    clock_t end = clock();
    double time_v55_matmul = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("   Time: %.3f seconds\n\n", time_v55_matmul);
    
    // Benchmark MatMul v5.6
    printf("⚡ Testing MatMul v5.6 (8x unroll + 4 acc)...\n");
    start = clock();
    for (int i = 0; i < iterations; i++) {
        matmul_v56(out, x, w, dim, hidden_dim);
    }
    end = clock();
    double time_v56_matmul = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("   Time: %.3f seconds\n", time_v56_matmul);
    printf("   Speedup: %.2fx\n\n", time_v55_matmul / time_v56_matmul);
    
    // Benchmark RMSNorm v5.5
    printf("🔄 Testing RMSNorm v5.5 (naive)...\n");
    start = clock();
    for (int i = 0; i < iterations * 10; i++) {
        rmsnorm_v55(out, x, weight, dim);
    }
    end = clock();
    double time_v55_rms = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("   Time: %.3f seconds\n\n", time_v55_rms);
    
    // Benchmark RMSNorm v5.6
    printf("⚡ Testing RMSNorm v5.6 (4x unroll)...\n");
    start = clock();
    for (int i = 0; i < iterations * 10; i++) {
        rmsnorm_v56(out, x, weight, dim);
    }
    end = clock();
    double time_v56_rms = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("   Time: %.3f seconds\n", time_v56_rms);
    printf("   Speedup: %.2fx\n\n", time_v56_rms / time_v55_rms);
    
    // Overall results
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║  RESULTS                                       ║\n");
    printf("╠════════════════════════════════════════════════╣\n");
    printf("║  MatMul Speedup:   %.2fx                       ║\n", time_v55_matmul / time_v56_matmul);
    printf("║  RMSNorm Speedup:  %.2fx                       ║\n", time_v55_rms / time_v56_rms);
    printf("║                                                ║\n");
    printf("║  Estimated Total:  ~%.2fx faster              ║\n", 
           (time_v55_matmul + time_v55_rms) / (time_v56_matmul + time_v56_rms));
    printf("╚════════════════════════════════════════════════╝\n");
    
    free(x);
    free(w);
    free(out);
    free(weight);
    
    return 0;
}
