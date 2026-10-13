#include "uuid.h"
#include "catch.hpp"

#include <cstring>
#include <set>
#include <unordered_set>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <iterator>

using namespace uuids;

// Whenever the standard library advertises std::span, the library must be using it instead of falling back to gsl::span.
// Detection once keyed off the value of __cplusplus, which silently picked gsl::span on compilers whose C++20 support predates that value being bumped; 
// a check for one such compiler version would not have covered the next one.
#ifdef __cpp_lib_span
static_assert(std::is_same_v<uuids::span<std::byte const, 16>,
                             std::span<std::byte const, 16>>,
              "std::span is available but stduuid fell back to gsl::span");
#endif

namespace
{
   // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0205r0.html
   template <typename EngineT, std::size_t StateSize = EngineT::state_size>
   void seed_rng(EngineT& engine)
   {
      using engine_type = typename EngineT::result_type;
      using device_type = std::random_device::result_type;
      using seedseq_type = std::seed_seq::result_type;
      constexpr auto bytes_needed = StateSize * sizeof(engine_type);
      constexpr auto numbers_needed = (sizeof(device_type) < sizeof(seedseq_type))
         ? (bytes_needed / sizeof(device_type))
         : (bytes_needed / sizeof(seedseq_type));
      std::array<device_type, numbers_needed> numbers{};
      std::random_device rnddev{};
      std::generate(std::begin(numbers), std::end(numbers), std::ref(rnddev));
      std::seed_seq seedseq(std::cbegin(numbers), std::cend(numbers));
      engine.seed(seedseq);
   }
}

TEST_CASE("Test default constructor", "[ctors]") 
{
   uuid empty;
   REQUIRE(empty.is_nil());
}

TEST_CASE("Test string conversion", "[ops]")
{
   uuid empty;
   REQUIRE(uuids::to_string(empty) == "00000000-0000-0000-0000-000000000000");
   REQUIRE(uuids::to_string<wchar_t>(empty) == L"00000000-0000-0000-0000-000000000000");
}

TEST_CASE("Test is_valid_uuid(char*)", "[parse]")
{
   REQUIRE(uuids::uuid::is_valid_uuid("47183823-2574-4bfd-b411-99ed177d3e43"));
   REQUIRE(uuids::uuid::is_valid_uuid("{47183823-2574-4bfd-b411-99ed177d3e43}"));
   REQUIRE(uuids::uuid::is_valid_uuid(L"47183823-2574-4bfd-b411-99ed177d3e43"));
   REQUIRE(uuids::uuid::is_valid_uuid(L"{47183823-2574-4bfd-b411-99ed177d3e43}"));
   REQUIRE(uuids::uuid::is_valid_uuid("00000000-0000-0000-0000-000000000000"));
   REQUIRE(uuids::uuid::is_valid_uuid("{00000000-0000-0000-0000-000000000000}"));
   REQUIRE(uuids::uuid::is_valid_uuid(L"00000000-0000-0000-0000-000000000000"));
   REQUIRE(uuids::uuid::is_valid_uuid(L"{00000000-0000-0000-0000-000000000000}"));
}

TEST_CASE("Test is_valid_uuid(basic_string)", "[parse]")
{
   using namespace std::string_literals;

   {
      auto str = "47183823-2574-4bfd-b411-99ed177d3e43"s;
      REQUIRE(uuids::uuid::is_valid_uuid(str));
   }

   {
      auto str = "{47183823-2574-4bfd-b411-99ed177d3e43}"s;
      REQUIRE(uuids::uuid::is_valid_uuid(str));
   }

   {
      auto str = L"47183823-2574-4bfd-b411-99ed177d3e43"s;
      REQUIRE(uuids::uuid::is_valid_uuid(str));
   }

   {
      auto str = L"{47183823-2574-4bfd-b411-99ed177d3e43}"s;
      REQUIRE(uuids::uuid::is_valid_uuid(str));
   }

   {
      auto str = "00000000-0000-0000-0000-000000000000"s;
      REQUIRE(uuids::uuid::is_valid_uuid(str));
   }

   {
      auto str = "{00000000-0000-0000-0000-000000000000}"s;
      REQUIRE(uuids::uuid::is_valid_uuid(str));
   }

   {
      auto str = L"00000000-0000-0000-0000-000000000000"s;
      REQUIRE(uuids::uuid::is_valid_uuid(str));
   }

   {
      auto str = L"{00000000-0000-0000-0000-000000000000}"s;
      REQUIRE(uuids::uuid::is_valid_uuid(str));
   }
}

TEST_CASE("Test is_valid_uuid(basic_string_view)", "[parse]")
{
   using namespace std::string_view_literals;

   REQUIRE(uuids::uuid::is_valid_uuid("47183823-2574-4bfd-b411-99ed177d3e43"sv));
   REQUIRE(uuids::uuid::is_valid_uuid("{47183823-2574-4bfd-b411-99ed177d3e43}"sv));
   REQUIRE(uuids::uuid::is_valid_uuid(L"47183823-2574-4bfd-b411-99ed177d3e43"sv));
   REQUIRE(uuids::uuid::is_valid_uuid(L"{47183823-2574-4bfd-b411-99ed177d3e43}"sv));
   REQUIRE(uuids::uuid::is_valid_uuid("00000000-0000-0000-0000-000000000000"sv));
   REQUIRE(uuids::uuid::is_valid_uuid("{00000000-0000-0000-0000-000000000000}"sv));
   REQUIRE(uuids::uuid::is_valid_uuid(L"00000000-0000-0000-0000-000000000000"sv));
   REQUIRE(uuids::uuid::is_valid_uuid(L"{00000000-0000-0000-0000-000000000000}"sv));
}

TEST_CASE("Test is_valid_uuid(char*) invalid format", "[parse]")
{
   REQUIRE(!uuids::uuid::is_valid_uuid(""));
   REQUIRE(!uuids::uuid::is_valid_uuid("{}"));
   REQUIRE(!uuids::uuid::is_valid_uuid("47183823-2574-4bfd-b411-99ed177d3e4"));
   REQUIRE(!uuids::uuid::is_valid_uuid("47183823-2574-4bfd-b411-99ed177d3e430"));
   REQUIRE(!uuids::uuid::is_valid_uuid("{47183823-2574-4bfd-b411-99ed177d3e43"));
   REQUIRE(!uuids::uuid::is_valid_uuid("47183823-2574-4bfd-b411-99ed177d3e43}"));
}

TEST_CASE("Test is_valid_uuid(basic_string) invalid format", "[parse]")
{
   using namespace std::string_literals;

   {
      auto str = ""s;
      REQUIRE(!uuids::uuid::is_valid_uuid(str));
   }

   {
      auto str = "{}"s;
      REQUIRE(!uuids::uuid::is_valid_uuid(str));
   }

   {
      auto str = "47183823-2574-4bfd-b411-99ed177d3e4"s;
      REQUIRE(!uuids::uuid::is_valid_uuid(str));
   }

   {
      auto str = "47183823-2574-4bfd-b411-99ed177d3e430"s;
      REQUIRE(!uuids::uuid::is_valid_uuid(str));
   }

   {
      auto str = "{47183823-2574-4bfd-b411-99ed177d3e43"s;
      REQUIRE(!uuids::uuid::is_valid_uuid(str));
   }

   {
      auto str = "47183823-2574-4bfd-b411-99ed177d3e43}"s;
      REQUIRE(!uuids::uuid::is_valid_uuid(str));
   }
}

TEST_CASE("Test is_valid_uuid(basic_string_view) invalid format", "[parse]")
{
   using namespace std::string_view_literals;

   REQUIRE(!uuids::uuid::is_valid_uuid(""sv));
   REQUIRE(!uuids::uuid::is_valid_uuid("{}"sv));
   REQUIRE(!uuids::uuid::is_valid_uuid("47183823-2574-4bfd-b411-99ed177d3e4"sv));
   REQUIRE(!uuids::uuid::is_valid_uuid("47183823-2574-4bfd-b411-99ed177d3e430"sv));
   REQUIRE(!uuids::uuid::is_valid_uuid("{47183823-2574-4bfd-b411-99ed177d3e43"sv));
   REQUIRE(!uuids::uuid::is_valid_uuid("47183823-2574-4bfd-b411-99ed177d3e43}"sv));
}

TEST_CASE("Test from_string(char*)", "[parse]")
{
   {
      auto str = "47183823-2574-4bfd-b411-99ed177d3e43";
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(uuids::to_string(guid) == str);
   }

   {
      auto str = "{47183823-2574-4bfd-b411-99ed177d3e43}";
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43");
   }

   {
      auto guid = uuids::uuid::from_string("47183823-2574-4bfd-b411-99ed177d3e43").value();
      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43");
      REQUIRE(uuids::to_string<wchar_t>(guid) == L"47183823-2574-4bfd-b411-99ed177d3e43");
   }

   {
      auto str = L"47183823-2574-4bfd-b411-99ed177d3e43";
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(uuids::to_string<wchar_t>(guid) == str);
   }

   {
      auto str = "4718382325744bfdb41199ed177d3e43";
      REQUIRE_NOTHROW(uuids::uuid::from_string(str));
      REQUIRE(uuids::uuid::from_string(str).has_value());
   }

   {
      auto str = "00000000-0000-0000-0000-000000000000";
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(guid.is_nil());
   }

   {
      auto str = "{00000000-0000-0000-0000-000000000000}";
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(guid.is_nil());
   }

   {
      auto str = L"00000000-0000-0000-0000-000000000000";
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(guid.is_nil());
   }

   {
      auto str = L"{00000000-0000-0000-0000-000000000000}";
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(guid.is_nil());
   }
}

TEST_CASE("Test from_string(basic_string)", "[parse]")
{
   using namespace std::string_literals;

   {
      auto str = "47183823-2574-4bfd-b411-99ed177d3e43"s;
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(uuids::to_string(guid) == str);
   }

   {
      auto str = "{47183823-2574-4bfd-b411-99ed177d3e43}"s;
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43");
   }

   {
      auto guid = uuids::uuid::from_string("47183823-2574-4bfd-b411-99ed177d3e43"s).value();
      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43");
      REQUIRE(uuids::to_string<wchar_t>(guid) == L"47183823-2574-4bfd-b411-99ed177d3e43");
   }

   {
      auto str = L"47183823-2574-4bfd-b411-99ed177d3e43"s;
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(uuids::to_string<wchar_t>(guid) == str);
   }

   {
      auto str = "4718382325744bfdb41199ed177d3e43"s;
      REQUIRE_NOTHROW(uuids::uuid::from_string(str));
      REQUIRE(uuids::uuid::from_string(str).has_value());
   }

   {
      auto str = "00000000-0000-0000-0000-000000000000"s;
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(guid.is_nil());
   }

   {
      auto str = "{00000000-0000-0000-0000-000000000000}"s;
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(guid.is_nil());
   }

   {
      auto str = L"00000000-0000-0000-0000-000000000000"s;
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(guid.is_nil());
   }

   {
      auto str = L"{00000000-0000-0000-0000-000000000000}"s;
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(guid.is_nil());
   }
}

TEST_CASE("Test from_string(basic_string_view)", "[parse]")
{
   using namespace std::string_view_literals;

   {
      auto str = "47183823-2574-4bfd-b411-99ed177d3e43"sv;
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(uuids::to_string(guid) == str);
   }

   {
      auto str = "{47183823-2574-4bfd-b411-99ed177d3e43}"sv;
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43");
   }

   {
      auto guid = uuids::uuid::from_string("47183823-2574-4bfd-b411-99ed177d3e43"sv).value();
      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43");
      REQUIRE(uuids::to_string<wchar_t>(guid) == L"47183823-2574-4bfd-b411-99ed177d3e43");
   }

   {
      auto str = L"47183823-2574-4bfd-b411-99ed177d3e43"sv;
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(uuids::to_string<wchar_t>(guid) == str);
   }

   {
      auto str = "4718382325744bfdb41199ed177d3e43"sv;
      REQUIRE_NOTHROW(uuids::uuid::from_string(str));
      REQUIRE(uuids::uuid::from_string(str).has_value());
   }

   {
      auto str = "00000000-0000-0000-0000-000000000000"sv;
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(guid.is_nil());
   }

   {
      auto str = "{00000000-0000-0000-0000-000000000000}"sv;
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(guid.is_nil());
   }

   {
      auto str = L"00000000-0000-0000-0000-000000000000"sv;
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(guid.is_nil());
   }

   {
      auto str = L"{00000000-0000-0000-0000-000000000000}"sv;
      auto guid = uuids::uuid::from_string(str).value();
      REQUIRE(guid.is_nil());
   }
}

TEST_CASE("Test constexpr from_string", "[const]")
{
   constexpr uuid value = uuid::from_string("47183823-2574-4bfd-b411-99ed177d3e43").value();
   static_assert(!value.is_nil());
   static_assert(value.variant() == uuid_variant::rfc);
   static_assert(value.version() != uuid_version::none);
}

TEST_CASE("Test from_string(char*) invalid format", "[parse]")
{
   REQUIRE(!uuids::uuid::from_string("").has_value());
   REQUIRE(!uuids::uuid::from_string("{}").has_value());
   REQUIRE(!uuids::uuid::from_string("47183823-2574-4bfd-b411-99ed177d3e4").has_value());
   REQUIRE(!uuids::uuid::from_string("47183823-2574-4bfd-b411-99ed177d3e430").has_value());
   REQUIRE(!uuids::uuid::from_string("{47183823-2574-4bfd-b411-99ed177d3e43").has_value());
   REQUIRE(!uuids::uuid::from_string("47183823-2574-4bfd-b411-99ed177d3e43}").has_value());
}

TEST_CASE("Test from_string(basic_string) invalid format", "[parse]")
{
   using namespace std::string_literals;

   {
      auto str = ""s;
      REQUIRE(!uuids::uuid::from_string(str).has_value());
   }

   {
      auto str = "{}"s;
      REQUIRE(!uuids::uuid::from_string(str).has_value());
   }

   {
      auto str = "47183823-2574-4bfd-b411-99ed177d3e4"s;
      REQUIRE(!uuids::uuid::from_string(str).has_value());
   }

   {
      auto str = "47183823-2574-4bfd-b411-99ed177d3e430"s;
      REQUIRE(!uuids::uuid::from_string(str).has_value());
   }

   {
      auto str = "{47183823-2574-4bfd-b411-99ed177d3e43"s;
      REQUIRE(!uuids::uuid::from_string(str).has_value());
   }

   {
      auto str = "47183823-2574-4bfd-b411-99ed177d3e43}"s;
      REQUIRE(!uuids::uuid::from_string(str).has_value());
   }
}

TEST_CASE("Test from_string(basic_string_view) invalid format", "[parse]")
{
   using namespace std::string_view_literals;

   REQUIRE(!uuids::uuid::from_string(""sv).has_value());
   REQUIRE(!uuids::uuid::from_string("{}"sv).has_value());
   REQUIRE(!uuids::uuid::from_string("47183823-2574-4bfd-b411-99ed177d3e4"sv).has_value());
   REQUIRE(!uuids::uuid::from_string("47183823-2574-4bfd-b411-99ed177d3e430"sv).has_value());
   REQUIRE(!uuids::uuid::from_string("{47183823-2574-4bfd-b411-99ed177d3e43"sv).has_value());
   REQUIRE(!uuids::uuid::from_string("47183823-2574-4bfd-b411-99ed177d3e43}"sv).has_value());
}

TEST_CASE("Test iterators constructor", "[ctors]")
{
   using namespace std::string_literals;

   {
      std::array<uuids::uuid::value_type, 16> arr{ {
            0x47, 0x18, 0x38, 0x23,
            0x25, 0x74,
            0x4b, 0xfd,
            0xb4, 0x11,
            0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43 } };

      uuid guid(std::begin(arr), std::end(arr));
      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43"s);
   }

   {
      uuids::uuid::value_type arr[16] = {
         0x47, 0x18, 0x38, 0x23,
         0x25, 0x74,
         0x4b, 0xfd,
         0xb4, 0x11,
         0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43 };

      uuid guid(std::begin(arr), std::end(arr));
      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43"s);
   }
}

TEST_CASE("Test array constructors", "[ctors]")
{
   using namespace std::string_literals;

   {
      uuids::uuid guid{
         {0x47, 0x18, 0x38, 0x23,
          0x25, 0x74,
          0x4b, 0xfd,
          0xb4, 0x11,
          0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43 } };

      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43"s);
   }

   {
      std::array<uuids::uuid::value_type, 16> arr{ {
            0x47, 0x18, 0x38, 0x23,
            0x25, 0x74,
            0x4b, 0xfd,
            0xb4, 0x11,
            0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43 } };

      uuid guid(arr);
      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43"s);
   }

   {
      uuids::uuid::value_type arr[16] {
            0x47, 0x18, 0x38, 0x23,
            0x25, 0x74,
            0x4b, 0xfd,
            0xb4, 0x11,
            0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43 };

      uuid guid(arr);
      REQUIRE(uuids::to_string(guid) == "47183823-2574-4bfd-b411-99ed177d3e43"s);
   }
}

TEST_CASE("Test equality", "[operators]")
{
   uuid empty;

   auto engine = uuids::uuid_random_generator::engine_type{};
   seed_rng(engine);
   uuid guid = uuids::uuid_random_generator{engine}();

   REQUIRE(empty == empty);
   REQUIRE(guid == guid);
   REQUIRE(empty != guid);
}

TEST_CASE("Test comparison", "[operators]")
{
   auto empty = uuid{};

   auto engine = uuids::uuid_random_generator::engine_type{};
   seed_rng(engine);

   uuids::uuid_random_generator gen{ engine };
   auto id = gen();

   REQUIRE(empty < id);

   std::set<uuids::uuid> ids{
      uuid{},
      gen(),
      gen(),
      gen(),
      gen()
   };

   REQUIRE(ids.size() == 5);
   REQUIRE(ids.find(uuid{}) != ids.end());
}

TEST_CASE("Test hashing", "[ops]")
{
   using namespace std::string_literals;
   auto str = "47183823-2574-4bfd-b411-99ed177d3e43"s;
   auto guid = uuids::uuid::from_string(str).value();

   auto h1 = std::hash<std::string>{};
   auto h2 = std::hash<uuid>{};
#ifdef UUID_HASH_STRING_BASED
   REQUIRE(h1(str) == h2(guid));
#else
   REQUIRE(h1(str) != h2(guid));
#endif

   auto engine = uuids::uuid_random_generator::engine_type{};
   seed_rng(engine);
   uuids::uuid_random_generator gen{ engine };

   std::unordered_set<uuids::uuid> ids{
      uuid{},
      gen(),
      gen(),
      gen(),
      gen()
   };

   REQUIRE(ids.size() == 5);
   REQUIRE(ids.find(uuid{}) != ids.end());
}

TEST_CASE("Test swap", "[ops]")
{
   uuid empty;

   auto engine = uuids::uuid_random_generator::engine_type{};
   seed_rng(engine);
   uuid guid = uuids::uuid_random_generator{engine}();

   REQUIRE(empty.is_nil());
   REQUIRE(!guid.is_nil());

   std::swap(empty, guid);

   REQUIRE(!empty.is_nil());
   REQUIRE(guid.is_nil());

   empty.swap(guid);

   REQUIRE(empty.is_nil());
   REQUIRE(!guid.is_nil());
}

TEST_CASE("Test constexpr", "[const]")
{
   constexpr uuid empty;
   static_assert(empty.is_nil());
   static_assert(empty.variant() == uuid_variant::ncs);
   static_assert(empty.version() == uuid_version::none);
}

TEST_CASE("Test size", "[operators]")
{
   REQUIRE(sizeof(uuid) == 16);
}

TEST_CASE("Test assignment", "[ops]")
{
   auto id1 = uuids::uuid::from_string("47183823-2574-4bfd-b411-99ed177d3e43").value();
   auto id2 = id1;
   REQUIRE(id1 == id2);

   id1 = uuids::uuid::from_string("{fea43102-064f-4444-adc2-02cec42623f8}").value();
   REQUIRE(id1 != id2);

   auto id3 = std::move(id2);
   REQUIRE(uuids::to_string(id3) == "47183823-2574-4bfd-b411-99ed177d3e43");
}

TEST_CASE("Test trivial", "[trivial]")
{
   REQUIRE(std::is_trivially_copyable_v<uuids::uuid>);
}

TEST_CASE("Test as_bytes", "[ops]")
{
   std::array<uuids::uuid::value_type, 16> arr{ {
         0x47, 0x18, 0x38, 0x23,
         0x25, 0x74,
         0x4b, 0xfd,
         0xb4, 0x11,
         0x99, 0xed, 0x17, 0x7d, 0x3e, 0x43
      } };

   {
      uuids::uuid id{ arr };
      REQUIRE(!id.is_nil());

      auto view = id.as_bytes();
      REQUIRE(memcmp(view.data(), arr.data(), arr.size()) == 0);
   }

   {
      const uuids::uuid id{ arr };
      REQUIRE(!id.is_nil());

      auto view = id.as_bytes();
      REQUIRE(memcmp(view.data(), arr.data(), arr.size()) == 0);
   }

}

#ifdef STDUUID_USE_STD_FORMAT
TEST_CASE("Test std::format", "[format]")
{
   auto id = uuids::uuid::from_string("47183823-2574-4bfd-b411-99ed177d3e43").value();
   auto str = uuids::to_string(id);
   auto wstr = uuids::to_string<wchar_t>(id);

   SECTION("no format specifier")
   {
      REQUIRE(std::format("{}", id) == str);
      REQUIRE(std::format(L"{}", id) == wstr);
      REQUIRE(std::format("{}", uuid{}) == "00000000-0000-0000-0000-000000000000");
   }

   SECTION("width, fill and align")
   {
      REQUIRE(std::format("{:>38}", id) == "  " + str);
      REQUIRE(std::format("{:<38}", id) == str + "  ");
      REQUIRE(std::format("{:^40}", id) == "  " + str + "  ");
      REQUIRE(std::format("{:*>38}", id) == "**" + str);
      REQUIRE(std::format("{:*<38}", id) == str + "**");
      REQUIRE(std::format(L"{:*>38}", id) == L"**" + wstr);
   }

   SECTION("width narrower than the uuid is ignored")
   {
      REQUIRE(std::format("{:10}", id) == str);
   }

   SECTION("several uuids in one call")
   {
      REQUIRE(std::format("{} {}", id, uuid{}) ==
              str + " 00000000-0000-0000-0000-000000000000");
   }
}
#endif

TEST_CASE("Test to_string with an output iterator", "[ops]")
{
   auto const expected = std::string{ "47183823-2574-4bfd-b411-99ed177d3e43" };
   auto id = uuids::uuid::from_string(expected).value();

   SECTION("appending to an existing buffer")
   {
      std::string buffer{ "id=" };
      uuids::to_string(id, std::back_inserter(buffer));
      REQUIRE(buffer == "id=" + expected);
   }

   SECTION("writing to an array")
   {
      char buffer[36];
      auto end = uuids::to_string(id, buffer);
      REQUIRE(end == buffer + 36);
      REQUIRE(std::string(buffer, 36) == expected);
   }

   SECTION("wide characters")
   {
      std::wstring buffer;
      uuids::to_string<wchar_t>(id, std::back_inserter(buffer));
      REQUIRE(buffer == L"47183823-2574-4bfd-b411-99ed177d3e43");
   }

   SECTION("nil uuid")
   {
      std::string buffer;
      uuids::to_string(uuid{}, std::back_inserter(buffer));
      REQUIRE(buffer == "00000000-0000-0000-0000-000000000000");
   }

   SECTION("agrees with the string returning overload")
   {
      auto engine = uuids::uuid_random_generator::engine_type{};
      seed_rng(engine);
      uuids::uuid_random_generator gen{ engine };

      for (int i = 0; i < 100; ++i)
      {
         auto const generated = gen();

         std::string narrow;
         uuids::to_string(generated, std::back_inserter(narrow));
         REQUIRE(narrow == uuids::to_string(generated));

         std::wstring wide;
         uuids::to_string<wchar_t>(generated, std::back_inserter(wide));
         REQUIRE(wide == uuids::to_string<wchar_t>(generated));
      }
   }
}

TEST_CASE("Test writing to an output stream", "[ops]")
{
   auto const expected = std::string{ "47183823-2574-4bfd-b411-99ed177d3e43" };
   auto id = uuids::uuid::from_string(expected).value();

   SECTION("narrow characters")
   {
      std::ostringstream os;
      os << id;
      REQUIRE(os.str() == expected);
   }

   SECTION("wide characters")
   {
      std::wostringstream os;
      os << id;
      REQUIRE(os.str() == L"47183823-2574-4bfd-b411-99ed177d3e43");
   }

   SECTION("nil uuid")
   {
      std::ostringstream os;
      os << uuid{};
      REQUIRE(os.str() == "00000000-0000-0000-0000-000000000000");
   }

   SECTION("stream width and fill are honoured")
   {
      std::ostringstream os;
      os << std::setw(40) << std::setfill('*') << id;
      REQUIRE(os.str() == "****" + expected);
   }

   SECTION("part of a larger stream")
   {
      std::ostringstream os;
      os << "id=" << id << ";";
      REQUIRE(os.str() == "id=" + expected + ";");
   }
}
