#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>
#include <omp.h>

#define FILE_NAME "data.csv"
#define TOL 0.0001
#define MAX_ITER 100
#define ERRCODE 2
#define ERR(e) {printf("Error: %s\n", nc_strerror(e)); exit(ERRCODE);}

float** Make2DFloatArray(int rows, int cols) {
    float *data = (float *)malloc(rows * cols * sizeof(float));
    float **array = (float **)malloc(rows * sizeof(float *));
    for (int i = 0; i < rows; i++)
        array[i] = &(data[cols * i]);

    return array;
}

float distance(float x1, float y1, float x2, float y2) {
    return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

int readX(float*** p_X, int* p_N_samples, int* p_N_features) {
    FILE* file = fopen(FILE_NAME, "r");
    if (file == NULL) {
        printf("Error: Could not open the file.\n");
        exit(ERRCODE);
    }

    int N_samples = 0;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        N_samples++;
    }

    fseek(file, 0, SEEK_SET);

    int N_features = 0;
    fgets(buffer, sizeof(buffer), file);
    char* token = strtok(buffer, ",");
    while (token != NULL) {
        N_features++;
        token = strtok(NULL, ",");
    }

    fseek(file, 0, SEEK_SET);

    *p_X = Make2DFloatArray(N_samples, N_features);

    for (int i = 0; i < N_samples; i++) {
        for (int j = 0; j < N_features; j++) {
            fscanf(file, "%f", &(*p_X)[i][j]);
            if (j < N_features - 1) {
                fgetc(file);  // Skip the comma
            }
        }
    }

    fclose(file);

    *p_N_samples = N_samples;
    *p_N_features = N_features;

    return 0;
}

void kMeans(float** X, int N_samples, int N_features, int N_clusters, int N_repeat, int** p_labels, float*** p_cluster_centers) {
    // Allocate memory for cluster centers
    float** cluster_centers = Make2DFloatArray(N_clusters, N_features);

    // Allocate memory for cluster assignments
    int* labels = (int *)malloc(N_samples * sizeof(int));

    // Randomly initialize cluster centers
    
    for (int i = 0; i < N_clusters; i++) {
        int random_idx = rand() % N_samples;
        for (int j = 0; j < N_features; j++) {
            cluster_centers[i][j] = X[random_idx][j];
        }
    }
  
double start_time = omp_get_wtime();
    // K-means clustering
 #pragma omp parallel
{
    for (int iter = 0; iter < MAX_ITER; iter++) {
        // Assign each data point to the nearest cluster center
        #pragma omp for
        for (int i = 0; i < N_samples; i++) {
            float min_dist = distance(X[i][0], X[i][1], cluster_centers[0][0], cluster_centers[0][1]);
            int min_cluster = 0;
            for (int k = 1; k < N_clusters; k++) {
                float dist = distance(X[i][0], X[i][1], cluster_centers[k][0], cluster_centers[k][1]);
                if (dist < min_dist) {
                    min_dist = dist;
                    min_cluster = k;
                }
            }
            labels[i] = min_cluster;
        }

        // Update cluster centers
        #pragma omp for
        for (int k = 0; k < N_clusters; k++) {
            float sum_x = 0.0, sum_y = 0.0;
            int count = 0;
            #pragma omp parallel for reduction(+:sum_x, sum_y, count)
            for (int i = 0; i < N_samples; i++) {
                if (labels[i] == k) {
                    sum_x += X[i][0];
                    sum_y += X[i][1];
                    count++;
                }
            }
            if (count > 0) {
                #pragma omp critical
                {
                    cluster_centers[k][0] = sum_x / count;
                    cluster_centers[k][1] = sum_y / count;
                }
            }
        }
    }
}

    double end_time = omp_get_wtime();
    printf("Time taken for kMeans: %f seconds\n", end_time - start_time);

    *p_labels = labels;
    *p_cluster_centers = cluster_centers;
}

int main() {
    // Initialization
    int N_samples, N_features, N_clusters = 10, N_repeat = 10;
    float** X;

    // Get the maximum number of threads available
    int max_threads = omp_get_max_threads();
    printf("Maximum number of threads available: %d\n", max_threads);

    // Prompt the user to enter the number of threads to use
    int num_threads;
    printf("Enter the number of threads to use (1 to %d): ", max_threads);
    scanf("%d", &num_threads);

    // Set the number of threads
    omp_set_num_threads(num_threads);

    // Record start time for the entire program
    double start_total_time = omp_get_wtime();

    // Read data from CSV file
    double start_read_time = omp_get_wtime();
    readX(&X, &N_samples, &N_features);
    double end_read_time = omp_get_wtime();
    printf("Time taken for reading data: %f seconds\n", end_read_time - start_read_time);

    // Perform k-means clustering
    int* labels;
    float** cluster_centers;
    kMeans(X, N_samples, N_features, N_clusters, N_repeat, &labels, &cluster_centers);

    // Write cluster assignments to a CSV file
    double start_write_assign_time = omp_get_wtime();
    FILE* assignments_file = fopen("cluster_assignments.csv", "w");
    if (assignments_file == NULL) {
        printf("Error: Could not open the file for writing cluster assignments.\n");
        exit(ERRCODE);
    }

    for (int i = 0; i < N_samples; i++) {
        fprintf(assignments_file, "%d\n", labels[i]);
    }

    fclose(assignments_file);
    double end_write_assign_time = omp_get_wtime();
    printf("Time taken for writing assignments: %f seconds\n", end_write_assign_time - start_write_assign_time);

    // Write cluster centers to a CSV file
    double start_write_centers_time = omp_get_wtime();
    FILE* centers_file = fopen("cluster_centers.csv", "w");
    if (centers_file == NULL) {
        printf("Error: Could not open the file for writing cluster centers.\n");
        exit(ERRCODE);
    }

    for (int k = 0; k < N_clusters; k++) {
        fprintf(centers_file, "%f,%f\n", cluster_centers[k][0], cluster_centers[k][1]);
    }

    fclose(centers_file);
    double end_write_centers_time = omp_get_wtime();
    printf("Time taken for writing centers: %f seconds\n", end_write_centers_time - start_write_centers_time);

    // Record end time for the entire program
    double end_total_time = omp_get_wtime();
    printf("Total execution time: %f seconds\n", end_total_time - start_total_time);

    // Cleanup
    free(X[0]);
    free(X);
    free(labels);
    free(cluster_centers[0]);
    free(cluster_centers);

    return 0;
}


