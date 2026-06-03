#include "SandboxCore/array_utils.h"

#include "CoreMinimal.h"
#include "TestHarness.h"


TEST_CASE("Empty array is sorted", "[SandboxCore][Array]") {
    TArray<int32> empty;
    REQUIRE(ml::is_sorted_desc(empty));
}

TEST_CASE("Single element array is sorted", "[SandboxCore][Array]") {
    TArray<int32> array{1};
    REQUIRE(ml::is_sorted_desc(array));
}

TEST_CASE("Arrays not sorted", "[SandboxCore][Array]") {
    TArray<int32> array{};
    array.Reserve(10);
    
    SECTION("positive numbers") {
        array.Add(1);
        array.Add(2);
        array.Add(3);
        REQUIRE(!ml::is_sorted_desc(array));    
    }
    
    SECTION("negative numbers") {
        array.Add(-3);
        array.Add(-2);
        array.Add(-1);
        
        REQUIRE(!ml::is_sorted_desc(array));    
    }
    
    SECTION("negative numbers") {
        array.Add(-3);
        array.Add(-2);
        array.Add(-1);
        
        REQUIRE(!ml::is_sorted_desc(array));    
    }    
}

TEST_CASE("Arrays not sorted", "[SandboxCore][Array]") {
    TArray<int32> array{};
    array.Reserve(10);
    
    SECTION("single zero") {
        array.Add(0);       
        REQUIRE(ml::is_sorted_desc(array)); 
    }
    
    SECTION("two zeros") {
        array.Add(0);       
        array.Add(0);       
        REQUIRE(ml::is_sorted_desc(array)); 
    }
    
    SECTION("three positive values") {
        array.Add(3);       
        array.Add(2);       
        array.Add(1);       
        REQUIRE(ml::is_sorted_desc(array)); 
    }
}