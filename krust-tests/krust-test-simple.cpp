#define CATCH_CONFIG_MAIN 
#include "catch.hpp"

/* -----------------------------------------------------------------------------
 * Test of the RefObject class.
 */
#include "krust/public-api/ref-object.h"
namespace {
  namespace kr = Krust;
  struct TestRefObect : public kr::RefObject
  {
    TestRefObect(bool& destroyed) : mDestroyed(destroyed) {}
    ~TestRefObect() { mDestroyed = true; }
    bool& mDestroyed;
  };

  template<typename FutureContainer>
  void wait_all(FutureContainer& futures)
  {
    for(const auto& fut : futures){
      fut.wait();
    }
  }
}

TEST_CASE("RefObject", "[simple]")
{
  bool destroyed = false;
  TestRefObect* obj = new TestRefObect(destroyed);
  REQUIRE(destroyed == false);
  constexpr int count = 99;
  for (int i = 0; i < count; ++i)
  {
    obj->Inc();
    REQUIRE(obj->Count() == i + 1);
  }
  REQUIRE(destroyed == false);

  for (size_t i = obj->Count(); i > 1; --i)
  {
    obj->Dec();
    REQUIRE(obj->Count() == i - 1);
  }
  REQUIRE(destroyed == false);
  obj->Dec();
  REQUIRE(destroyed == true);
}

/// Slam a refcounted object from lots of threads at the same time.
#include <future>
TEST_CASE("RefObjectAsync", "[simple]")
{
  bool destroyed = false;
  TestRefObect* obj = new TestRefObect(destroyed);
  REQUIRE(destroyed == false);
  constexpr int count = 99;
  constexpr int inner_count = 500000;
  std::vector<std::future<void>> futures;
  for (int i = 0; i < count; ++i)
  {
    futures.push_back(std::async(std::launch::async, [obj](){
      for(unsigned j = 0; j < inner_count; ++j){
        obj->Inc();
      }
    }));
    obj->Inc();
  }
  wait_all(futures);
  REQUIRE(obj->Count() == count * inner_count + count);
  REQUIRE(destroyed == false);

  obj->Inc();
  futures.clear();
  for (int i = 0; i < count; ++i)
  {
    futures.push_back(std::async(std::launch::async, [obj](){
      for(unsigned j = 0; j < inner_count; ++j){
        obj->Dec();
      }
    }));
    obj->Dec();
  }
  wait_all(futures);
  REQUIRE(obj->Count() == 1);
  REQUIRE(destroyed == false);
  obj->Dec();
  REQUIRE(destroyed == true);
}

// Interleave incs and decs from multiple threads
TEST_CASE("RefObjectAsyncJumbled", "[simple]")
{
  bool destroyed = false;
  TestRefObect* obj = new TestRefObect(destroyed);
  REQUIRE(destroyed == false);
  constexpr int count = 99;
  constexpr int inner_count = 500000;

  // Prime the counts so we have a buffer to play with:
  for (int i = 0; i < count * inner_count; ++i)
  {
    obj->Inc();
  }

  // Launch battling incing and decing threads:
  std::vector<std::future<void>> futures;
  for (int i = 0; i < count; ++i)
  {
    futures.push_back(std::async(std::launch::async, [obj](){
      for(unsigned j = 0; j < inner_count; ++j){
        obj->Inc();
      }
    }));
    futures.push_back(std::async(std::launch::async, [obj](){
      for(unsigned j = 0; j < inner_count; ++j){
        obj->Dec();
      }
    }));
  }
  wait_all(futures);
  REQUIRE(obj->Count() == count * inner_count);
  REQUIRE(destroyed == false);
}

/* -----------------------------------------------------------------------------
 * Test of the keep-alive set class.
 */
#include "krust/internal/keep-alive-set.h"
TEST_CASE("KeepAliveSet", "[simple]")
{
  namespace kr = Krust;
  struct TestRefObect : public kr::RefObject
  {
    TestRefObect(size_t& count) : count(count) { ++count;  }
    ~TestRefObect() { --count; }
    size_t& count;
  };

  constexpr unsigned numObjects = 10000;
  size_t liveObjects = 0;
  
  SECTION(" Adding without duplicates ")
  {
    {
      kr::KeepAliveSet keepalives;
      for (unsigned i = 0; i < numObjects; ++i)
      {
        TestRefObect* tro = new TestRefObect(liveObjects);
        keepalives.Add(*tro);
      }
      REQUIRE(keepalives.Size() == numObjects);
      REQUIRE(liveObjects == numObjects);
    }
    REQUIRE(liveObjects == 0);
  }

  SECTION(" Adding with duplicates ")
  {
    {
      kr::KeepAliveSet keepalives2;
      std::vector<kr::IntrusivePointer<TestRefObect>> objects;
      for (unsigned i = 0; i < numObjects; ++i)
      {
        objects.push_back(new TestRefObect(liveObjects));
      }
      for (unsigned dup = 0; dup < 3; ++dup)
      {
        for (auto ptr : objects)
        {
          keepalives2.Add(*ptr);
        }
      }
      REQUIRE(keepalives2.Size() == numObjects);
      REQUIRE(liveObjects == numObjects);
      objects.clear();
      REQUIRE(liveObjects == numObjects);
    }
    REQUIRE(liveObjects == 0);
  }
}