#include "bloom_filter.h"
#include "test_framework.h"
#include <set>

class BloomFilterTest {
  public:
    BloomFilterTest() {
        setUp();
    }

    static void setUp() {
        // Tests will create their own filters as needed
    }
};

bool test_basic_add_and_contains(BloomFilterTest &fixture) {
    fixture.setUp();
    BloomFilter filter(100, 0.01);

    filter.add("key1");
    filter.add("key2");
    filter.add("key3");

    ASSERT_TRUE(filter.contains("key1"), "Filter should contain key1");
    ASSERT_TRUE(filter.contains("key2"), "Filter should contain key2");
    ASSERT_TRUE(filter.contains("key3"), "Filter should contain key3");

    return true;
}

bool test_does_not_contain_unadded_keys(BloomFilterTest &fixture) {
    fixture.setUp();
    BloomFilter filter(100, 0.01);

    filter.add("key1");
    filter.add("key2");

    ASSERT_TRUE(filter.contains("key1"), "Should not have false negative for added key");
    ASSERT_TRUE(filter.contains("key2"), "Should not have false negative for added key");

    return true;
}

bool test_false_positive_rate(BloomFilterTest &fixture) {
    fixture.setUp();
    size_t num_elements = 1000;
    double target_fpr = 0.01;
    BloomFilter filter(num_elements, target_fpr);

    // Add elements
    for (size_t i = 0; i < num_elements; i++) {
        filter.add("key" + std::to_string(i));
    }

    // Verify all added elements are present (no false negatives)
    for (size_t i = 0; i < num_elements; i++) {
        ASSERT_TRUE(filter.contains("key" + std::to_string(i)), "Bloom filter should not have false negatives");
    }

    // Test false positive rate with elements that weren't added
    size_t false_positives = 0;
    size_t test_samples = 10000;
    for (size_t i = num_elements; i < num_elements + test_samples; i++) {
        if (filter.contains("key" + std::to_string(i))) {
            false_positives++;
        }
    }

    double actual_fpr = static_cast<double>(false_positives) / test_samples;
    // Allow some margin (3x the target rate) since it's probabilistic
    ASSERT_TRUE(actual_fpr < target_fpr * 3.0, "False positive rate should be reasonably close to target");

    return true;
}

bool test_empty_filter(BloomFilterTest &fixture) {
    fixture.setUp();
    BloomFilter filter(100, 0.01);

    ASSERT_TRUE(!filter.contains("key1"), "Empty filter should not contain any keys");
    ASSERT_TRUE(!filter.contains("key2"), "Empty filter should not contain any keys");
    ASSERT_TRUE(!filter.contains(""), "Empty filter should not contain empty string");

    return true;
}

bool test_empty_string_key(BloomFilterTest &fixture) {
    fixture.setUp();
    BloomFilter filter(100, 0.01);

    filter.add("");
    ASSERT_TRUE(filter.contains(""), "Filter should contain empty string after adding it");
    ASSERT_TRUE(!filter.contains("key1"), "Filter should not contain other keys");

    return true;
}

bool test_duplicate_keys(BloomFilterTest &fixture) {
    fixture.setUp();
    BloomFilter filter(100, 0.01);

    filter.add("key1");
    filter.add("key1");
    filter.add("key1");

    ASSERT_TRUE(filter.contains("key1"), "Filter should contain key1 after multiple adds");

    return true;
}

bool test_special_characters(BloomFilterTest &fixture) {
    fixture.setUp();
    BloomFilter filter(100, 0.01);

    filter.add("key!@#$%");
    filter.add("key\n\t");
    filter.add("key with spaces");
    filter.add("key™️unicode");

    ASSERT_TRUE(filter.contains("key!@#$%"), "Filter should handle special characters");
    ASSERT_TRUE(filter.contains("key\n\t"), "Filter should handle whitespace characters");
    ASSERT_TRUE(filter.contains("key with spaces"), "Filter should handle spaces");
    ASSERT_TRUE(filter.contains("key™️unicode"), "Filter should handle unicode");

    return true;
}

bool test_long_keys(BloomFilterTest &fixture) {
    fixture.setUp();
    BloomFilter filter(100, 0.01);

    std::string long_key(10000, 'a');
    filter.add(long_key);

    ASSERT_TRUE(filter.contains(long_key), "Filter should handle very long keys");

    return true;
}

bool test_size(BloomFilterTest &fixture) {
    fixture.setUp();
    BloomFilter filter(100, 0.01);

    size_t size = filter.size();
    ASSERT_TRUE(size > 0, "Filter size should be positive");

    // Size should not change when adding elements
    filter.add("key1");
    filter.add("key2");
    ASSERT_EQ(filter.size(), size, "Filter size should remain constant after adding elements");

    return true;
}

bool test_serialize_empty_filter(BloomFilterTest &fixture) {
    fixture.setUp();
    BloomFilter filter(100, 0.01);

    std::vector<uint8_t> serialized = filter.serialize();
    ASSERT_TRUE(serialized.size() > 0, "Serialized data should not be empty");

    BloomFilter deserialized = BloomFilter::deserialize(serialized);
    ASSERT_EQ(deserialized.size(), filter.size(), "Deserialized filter should have same size");

    return true;
}

bool test_serialize_and_deserialize(BloomFilterTest &fixture) {
    fixture.setUp();
    BloomFilter filter(100, 0.01);

    // Add some keys
    filter.add("key1");
    filter.add("key2");
    filter.add("key3");
    filter.add("test_key");
    filter.add("another_key");

    // Serialize
    std::vector<uint8_t> serialized = filter.serialize();
    ASSERT_TRUE(serialized.size() > 0, "Serialized data should not be empty");

    // Deserialize
    BloomFilter deserialized = BloomFilter::deserialize(serialized);

    // Verify size matches
    ASSERT_EQ(deserialized.size(), filter.size(), "Deserialized filter should have same size");

    // Verify all keys are still present
    ASSERT_TRUE(deserialized.contains("key1"), "Deserialized filter should contain key1");
    ASSERT_TRUE(deserialized.contains("key2"), "Deserialized filter should contain key2");
    ASSERT_TRUE(deserialized.contains("key3"), "Deserialized filter should contain key3");
    ASSERT_TRUE(deserialized.contains("test_key"), "Deserialized filter should contain test_key");
    ASSERT_TRUE(deserialized.contains("another_key"), "Deserialized filter should contain another_key");

    return true;
}

bool test_serialize_with_many_elements(BloomFilterTest &fixture) {
    fixture.setUp();
    size_t num_elements = 1000;
    BloomFilter filter(num_elements, 0.01);

    // Add many elements
    for (size_t i = 0; i < num_elements; i++) {
        filter.add("key" + std::to_string(i));
    }

    // Serialize and deserialize
    std::vector<uint8_t> serialized = filter.serialize();
    BloomFilter deserialized = BloomFilter::deserialize(serialized);

    // Verify all elements are still present
    for (size_t i = 0; i < num_elements; i++) {
        ASSERT_TRUE(deserialized.contains("key" + std::to_string(i)), "Deserialized filter should contain all original keys");
    }

    return true;
}

bool test_different_false_positive_rates(BloomFilterTest &fixture) {
    fixture.setUp();

    // Lower false positive rate should result in larger filter
    BloomFilter filter1(100, 0.1);
    BloomFilter filter2(100, 0.01);
    BloomFilter filter3(100, 0.001);

    ASSERT_TRUE(filter1.size() < filter2.size(), "Lower FPR should result in larger bit array");
    ASSERT_TRUE(filter2.size() < filter3.size(), "Lower FPR should result in larger bit array");

    return true;
}

bool test_capacity_scaling(BloomFilterTest &fixture) {
    fixture.setUp();

    // More elements should result in larger filter
    BloomFilter filter1(100, 0.01);
    BloomFilter filter2(1000, 0.01);
    BloomFilter filter3(10000, 0.01);

    ASSERT_TRUE(filter1.size() < filter2.size(), "More elements should result in larger bit array");
    ASSERT_TRUE(filter2.size() < filter3.size(), "More elements should result in larger bit array");

    return true;
}

bool test_similar_keys(BloomFilterTest &fixture) {
    fixture.setUp();
    BloomFilter filter(100, 0.01);

    filter.add("key1");
    filter.add("key2");
    filter.add("key11");
    filter.add("key12");

    ASSERT_TRUE(filter.contains("key1"), "Filter should distinguish similar keys");
    ASSERT_TRUE(filter.contains("key2"), "Filter should distinguish similar keys");
    ASSERT_TRUE(filter.contains("key11"), "Filter should distinguish similar keys");
    ASSERT_TRUE(filter.contains("key12"), "Filter should distinguish similar keys");
    ASSERT_TRUE(!filter.contains("key3"), "Filter should not contain unadded similar key");

    return true;
}

void run_bloom_filter_tests(TestFramework &framework) {
    BloomFilterTest fixture;

    std::cout << "Running Bloom Filter Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    framework.run("test_basic_add_and_contains", [&]() { return test_basic_add_and_contains(fixture); });
    framework.run("test_does_not_contain_unadded_keys", [&]() { return test_does_not_contain_unadded_keys(fixture); });
    framework.run("test_false_positive_rate", [&]() { return test_false_positive_rate(fixture); });
    framework.run("test_empty_filter", [&]() { return test_empty_filter(fixture); });
    framework.run("test_empty_string_key", [&]() { return test_empty_string_key(fixture); });
    framework.run("test_duplicate_keys", [&]() { return test_duplicate_keys(fixture); });
    framework.run("test_special_characters", [&]() { return test_special_characters(fixture); });
    framework.run("test_long_keys", [&]() { return test_long_keys(fixture); });
    framework.run("test_size", [&]() { return test_size(fixture); });
    framework.run("test_serialize_empty_filter", [&]() { return test_serialize_empty_filter(fixture); });
    framework.run("test_serialize_and_deserialize", [&]() { return test_serialize_and_deserialize(fixture); });
    framework.run("test_serialize_with_many_elements", [&]() { return test_serialize_with_many_elements(fixture); });
    framework.run("test_different_false_positive_rates", [&]() { return test_different_false_positive_rates(fixture); });
    framework.run("test_capacity_scaling", [&]() { return test_capacity_scaling(fixture); });
    framework.run("test_similar_keys", [&]() { return test_similar_keys(fixture); });
}
