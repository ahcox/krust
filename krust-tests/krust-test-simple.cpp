#define CATCH_CONFIG_MAIN 
#include "catch.hpp"

TEST_CASE("trivial equalities", "[simple]")
{
  REQUIRE((1 * 1) == 1);
  REQUIRE((2 + 1) == 3);
  REQUIRE((3 * 2) == 6);
  REQUIRE((3 * 3) == 9);
}

#include "krust/public-api/ref-object.h"
TEST_CASE("RefObject", "[simple]")
{
  namespace kr = Krust;
  struct TestRefObect : public kr::RefObject
  {
    TestRefObect(bool& destroyed) : mDestroyed(destroyed) {}
    ~TestRefObect() { mDestroyed = true; }
    bool& mDestroyed;
  };

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
    REQUIRE(obj->Count() == i);
    obj->Dec();
    REQUIRE(obj->Count() == i - 1);
  }
  REQUIRE(destroyed == false);
  obj->Dec();
  REQUIRE(destroyed == true);
}

