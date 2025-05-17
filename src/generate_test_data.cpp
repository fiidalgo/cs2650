#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <algorithm>
#include <string>
#include <cstring>
#include <cmath>
#include <chrono>

struct KeyValuePair
{
    int64_t key;
    int64_t value;
};

void print_usage()
{
    std::cerr << "Usage: generate_test_data [OPTIONS]\n"
              << "Generate test data for LSM-tree benchmarking\n\n"
              << "Options:\n"
              << "  --size SIZE              Size of data to generate in MB (default: 100)\n"
              << "  --distribution DIST      Distribution type: 'uniform' or 'skewed' (default: uniform)\n"
              << "  --output FILEPATH        Output filepath (default: data.bin)\n"
              << "  --key-range RANGE        Key range multiplier (default: 2)\n"
              << "  --zipf-factor FACTOR     Zipf distribution skew factor for skewed dist (default: 1.2)\n"
              << "  --help                   Display this help message\n";
}

// Generate data with uniform distribution
std::vector<KeyValuePair> generate_uniform_data(size_t count, int64_t key_range)
{
    std::cout << "Generating uniform distribution data...\n";

    std::vector<KeyValuePair> data;
    data.reserve(count);

    // Use high-quality random generator
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<int64_t> key_dist(1, count * key_range);
    std::uniform_int_distribution<int64_t> value_dist(1, std::numeric_limits<int64_t>::max());

    // Generate unique keys
    std::vector<int64_t> keys;
    keys.reserve(count);

    for (size_t i = 0; i < count; i++)
    {
        keys.push_back(key_dist(gen));
    }

    // Sort and remove duplicates
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

    // If we have fewer unique keys than needed, generate more
    while (keys.size() < count)
    {
        size_t additional = count - keys.size();
        for (size_t i = 0; i < additional; i++)
        {
            keys.push_back(key_dist(gen));
        }
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    }

    // Truncate to exact count if we have more
    if (keys.size() > count)
    {
        keys.resize(count);
    }

    // Generate key-value pairs
    for (size_t i = 0; i < count; i++)
    {
        KeyValuePair pair;
        pair.key = keys[i];
        pair.value = value_dist(gen);
        data.push_back(pair);
    }

    return data;
}

// Generate data with skewed (Zipf) distribution
std::vector<KeyValuePair> generate_skewed_data(size_t count, int64_t key_range, double alpha)
{
    std::cout << "Generating skewed distribution data with Zipf alpha=" << alpha << "...\n";

    std::vector<KeyValuePair> data;
    data.reserve(count);

    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<int64_t> value_dist(1, std::numeric_limits<int64_t>::max());

    // Calculate Zipf distribution weights
    std::vector<double> weights;
    weights.reserve(count * key_range);

    double sum = 0.0;
    for (size_t i = 1; i <= count * key_range; i++)
    {
        double weight = 1.0 / std::pow(i, alpha);
        weights.push_back(weight);
        sum += weight;
    }

    // Normalize weights
    for (size_t i = 0; i < weights.size(); i++)
    {
        weights[i] /= sum;
    }

    // Create discrete distribution
    std::discrete_distribution<int64_t> key_dist(weights.begin(), weights.end());

    // Generate keys according to Zipf distribution
    std::vector<int64_t> keys;
    keys.reserve(count);

    for (size_t i = 0; i < count * 2; i++)
    {                                      // Generate more than needed to account for duplicates
        keys.push_back(key_dist(gen) + 1); // +1 because keys start from 1
    }

    // Sort and remove duplicates
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

    // Truncate to exact count if we have more
    if (keys.size() > count)
    {
        keys.resize(count);
    }

    // Generate more keys if needed
    while (keys.size() < count)
    {
        size_t additional = count - keys.size();
        for (size_t i = 0; i < additional * 2; i++)
        {
            keys.push_back(key_dist(gen) + 1);
        }
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

        if (keys.size() > count)
        {
            keys.resize(count);
        }
    }

    // Generate key-value pairs
    for (size_t i = 0; i < count; i++)
    {
        KeyValuePair pair;
        pair.key = keys[i];
        pair.value = value_dist(gen);
        data.push_back(pair);
    }

    return data;
}

int main(int argc, char *argv[])
{
    // Default parameters
    size_t size_mb = 100;
    std::string distribution = "uniform";
    std::string output_path = "data.bin";
    int64_t key_range_multiplier = 2;
    double zipf_factor = 1.2;

    // Parse command line arguments
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--size") == 0 && i + 1 < argc)
        {
            size_mb = std::stoul(argv[++i]);
        }
        else if (strcmp(argv[i], "--distribution") == 0 && i + 1 < argc)
        {
            distribution = argv[++i];
        }
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
        {
            output_path = argv[++i];
        }
        else if (strcmp(argv[i], "--key-range") == 0 && i + 1 < argc)
        {
            key_range_multiplier = std::stoll(argv[++i]);
        }
        else if (strcmp(argv[i], "--zipf-factor") == 0 && i + 1 < argc)
        {
            zipf_factor = std::stod(argv[++i]);
        }
        else if (strcmp(argv[i], "--help") == 0)
        {
            print_usage();
            return 0;
        }
        else
        {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage();
            return 1;
        }
    }

    // Validate parameters
    if (size_mb <= 0)
    {
        std::cerr << "Error: Size must be positive\n";
        return 1;
    }

    if (distribution != "uniform" && distribution != "skewed")
    {
        std::cerr << "Error: Distribution must be 'uniform' or 'skewed'\n";
        return 1;
    }

    // Calculate number of key-value pairs (each pair is 16 bytes: 8 for key, 8 for value)
    size_t pair_count = (size_mb * 1024 * 1024) / 16;

    std::cout << "Generating " << pair_count << " key-value pairs (" << size_mb << "MB) with "
              << distribution << " distribution\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    // Generate data based on distribution
    std::vector<KeyValuePair> data;
    if (distribution == "uniform")
    {
        data = generate_uniform_data(pair_count, key_range_multiplier);
    }
    else
    {
        data = generate_skewed_data(pair_count, key_range_multiplier, zipf_factor);
    }

    // Write data to binary file
    std::ofstream outfile(output_path, std::ios::binary);
    if (!outfile)
    {
        std::cerr << "Error: Could not open output file " << output_path << "\n";
        return 1;
    }

    // Write the count as a header
    uint64_t count = data.size();
    outfile.write(reinterpret_cast<char *>(&count), sizeof(count));

    // Write all key-value pairs
    for (const auto &pair : data)
    {
        outfile.write(reinterpret_cast<const char *>(&pair.key), sizeof(pair.key));
        outfile.write(reinterpret_cast<const char *>(&pair.value), sizeof(pair.value));
    }

    outfile.close();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "Generated " << count << " key-value pairs in " << duration << "ms\n";
    std::cout << "Data saved to " << output_path << "\n";

    return 0;
}