#include<iostream>
#include<stdlib.h>
#include<mpi.h>
#include<string.h>
#include<fstream>
#include<mpi.h>
#include<stdio.h>

using namespace std;

float euclidean_distance(float point[], float centre[], int d)
{
    int i;
    float distance = 0;
    for(i=1; i<d; i++)
    {
        distance = distance + ((point[i] - centre[i-1]) * (point[i] - centre[i-1]));
    }
    return distance/d;
}

float* assign_clusters(float **dataframe, float *min_distances, float *distances, int nsamples, int dims)
{
    int i, j, count = 0;
	float *mean_cluster = new float[dims-1];	
	for(i=0; i<dims; i++)
		mean_cluster[i] = 0;
	for(i=0; i<nsamples; i++)
	{
		if(min_distances[i] == distances[i])
		{	
			dataframe[i][0] = 1;
			for(j=0; j<dims-1; j++)
				mean_cluster[j] += dataframe[i][j+1];	
			count++;	
		}		
		else
			dataframe[i][0] = 0;
	}
	for(j=0; j<dims-1; j++)
		mean_cluster[j] /= count;
	return mean_cluster;
}

float** init_float_matrix(int rows, int cols)
{
    float **mat;
    mat = new float*[rows];
    for(int i = 0; i<rows; i++)
    {
        mat[i] = new float[cols];
    }
    return mat;
}

float** csv_to_float_matrix(char file_name[])
{
    ifstream csv_file;
    csv_file.open("data/pollution_new_small.csv", ios::in);
    char str[210];
    int start = 0, end, row_num = 0, col_num, first= 1;
    string row_values, substr, delim;
    float** feature_matrix = init_float_matrix(100000, 17);
    while(csv_file.getline(str, 210))
    {
        if(first)
        {
            first = 0;
            continue;
        }
        row_values = str;
        delim = ",";
        start = 0;
        end = row_values.find(delim);
        col_num = 0;
        while(end != string::npos)
        {
            substr =  row_values.substr(start, end - start);
            feature_matrix[row_num][col_num++] = stof(substr);
            start = end + delim.length();
            end = row_values.find(delim, start);
        }
        feature_matrix[row_num++][col_num++] = stof(row_values.substr(start, end));
    }
    return feature_matrix;
}

float* max_values(float **feauture_matrix, int nsamples, int dims)
{
    //Returns a float[dims-1] array containing maximum dimension value in all the samples.
    float *max_vals = new float[dims-1];
    float max;
    for(int j = 1; j < dims; j++)
    {
        max = 0.0;
        for(int i = 0; i < nsamples; i++)
        {
            if(max < feauture_matrix[i][j])
            {
                max = feauture_matrix[i][j];
            }
        }
        max_vals[j-1] = max;
    }
    return max_vals;
}

float* init_clusters(float *rand_max, int dims, int seed)
{
    float *clusters = new float[dims-1];
    for(int j = 0; j < dims-1; j++)
    {
        clusters[j] = rand_max[j] >= 1.0 ? (float) (rand() % (int) rand_max[j]) : 0.0;
    	clusters[j] = (clusters[j] * (seed^7)) / ((seed-1)^7);
    }
    return clusters;
}

float* points_distance(float **dataframe, float *cluster, int nsamples, int dims)
{
	int i;
	float *distances = new float[nsamples];
	for(i = 0; i < nsamples; i++)
	{
		distances[i] = euclidean_distance(dataframe[i], cluster, dims);
	}
	return distances;
}		

int main(int argc, char *argv[])
{
	int rank, size;
	MPI_Status status;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	int nsamples, dims;	
	float *new_cluster = new float[dims-1];	
	float *cluster_centre;
	float **dataframe;	
	float *distances;
	char file_name[] = "data/pollution_new_small.csv";
   	dataframe = csv_to_float_matrix(file_name);
    nsamples = 100000, dims = 17;
    float *rand_max = max_values(dataframe, nsamples, dims);
	MPI_Barrier(MPI_COMM_WORLD);
    cluster_centre = init_clusters(rand_max, dims, rank+2);
    int m;	
	for(m = 0; m < dims-1; m++)
		cout<< cluster_centre[m]<<" ";
	cout<<endl;	
	int changed = 1, ch = 0;
	float *min_distances = new float[nsamples];
	while(changed)
	{
		cout << "Iterating!\n";
		distances = points_distance(dataframe, cluster_centre, nsamples, dims);		
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Reduce(distances, min_distances, nsamples, MPI_FLOAT, MPI_MIN, 0, MPI_COMM_WORLD);
		MPI_Bcast(min_distances, nsamples, MPI_FLOAT, 0, MPI_COMM_WORLD);
		if(rank >= 0)
			new_cluster = assign_clusters(dataframe, min_distances, distances, nsamples, dims);
		MPI_Barrier(MPI_COMM_WORLD);
		ch = 0;	
		for(int i = 0; i < dims-1; i++)
		{
			if(new_cluster[i] != cluster_centre[i])
			{	
				ch = 1;
			}	
			cluster_centre[i] = new_cluster[i];
		}		
		MPI_Reduce(&ch, &changed, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
		fflush(stdout);
		MPI_Bcast(&changed, 1, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Barrier(MPI_COMM_WORLD);
	}
	int count = 0, i;
	for(i=0; i<nsamples; i++)
		if(dataframe[i][0] == 1)
			count++;
	cout<< "Cluster: "<< rank << " Points: "<<count<<endl;
	MPI_Finalize();
	return 0;
}