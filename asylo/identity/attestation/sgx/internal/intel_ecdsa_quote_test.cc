/*
 *
 * Copyright 2019 Asylo authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "asylo/identity/attestation/sgx/internal/intel_ecdsa_quote.h"

#include <algorithm>
#include <iterator>
#include <type_traits>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "asylo/crypto/util/byte_container_util.h"
#include "asylo/crypto/util/trivial_object_util.h"
#include "asylo/test/util/memory_matchers.h"
#include "asylo/test/util/status_matchers.h"

namespace asylo {
namespace sgx {
namespace {

using ::testing::ContainerEq;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Test;

class IntelEcdsaQuoteTest : public Test {
 protected:
  IntelQeQuote CreateRandomValidQuote() {
    IntelQeQuote quote;

    RandomFillTrivialObject(&quote.header);
    RandomFillTrivialObject(&quote.body);
    RandomFillTrivialObject(&quote.signature);
    AppendTrivialObject(TrivialRandomObject<UnsafeBytes<123>>(),
                        &quote.qe_authn_data);
    AppendTrivialObject(TrivialRandomObject<UnsafeBytes<456>>(),
                        &quote.cert_data.qe_cert_data);

    return quote;
  }

  void ExpectQuoteEquals(const StatusOr<IntelQeQuote> &actual_quote,
                         const IntelQeQuote &expected_quote) {
    ASYLO_ASSERT_OK(actual_quote);
    EXPECT_THAT(actual_quote.ValueOrDie().header,
                TrivialObjectEq(expected_quote.header));
    EXPECT_THAT(actual_quote.ValueOrDie().body,
                TrivialObjectEq(expected_quote.body));
    EXPECT_THAT(actual_quote.ValueOrDie().signature,
                TrivialObjectEq(expected_quote.signature));
    EXPECT_THAT(actual_quote.ValueOrDie().qe_authn_data,
                ContainerEq(expected_quote.qe_authn_data));
    EXPECT_THAT(actual_quote.ValueOrDie().cert_data.qe_cert_data_type,
                Eq(expected_quote.cert_data.qe_cert_data_type));
    EXPECT_THAT(actual_quote.ValueOrDie().cert_data.qe_cert_data,
                ContainerEq(expected_quote.cert_data.qe_cert_data));
  }
};

TEST_F(IntelEcdsaQuoteTest, ParseSuccess) {
  const IntelQeQuote kExpectedQuote = CreateRandomValidQuote();
  ExpectQuoteEquals(ParseDcapPackedQuote(PackDcapQuote(kExpectedQuote)),
                    kExpectedQuote);
}

TEST_F(IntelEcdsaQuoteTest, ParseQuoteSucceedsWithoutOptionalAuthnData) {
  IntelQeQuote expected_quote = CreateRandomValidQuote();
  expected_quote.qe_authn_data.clear();
  ExpectQuoteEquals(ParseDcapPackedQuote(PackDcapQuote(expected_quote)),
                    expected_quote);
}

TEST_F(IntelEcdsaQuoteTest, ParseQuoteFailsDueToInputBufferBeingTooLarge) {
  std::vector<uint8_t> packed_quote =
      PackDcapQuote(CreateRandomValidQuote());
  packed_quote.push_back('x');

  Status status = ParseDcapPackedQuote(packed_quote).status();
  EXPECT_THAT(status, StatusIs(error::GoogleError::INVALID_ARGUMENT));
  EXPECT_THAT(
      std::string(status.error_message().begin(), status.error_message().end()),
      HasSubstr("Expected signature data size of "));
}

TEST_F(IntelEcdsaQuoteTest, ParseQuoteFailsDueToInputBufferBeingTooSmall) {
  std::vector<uint8_t> packed_quote =
      PackDcapQuote(CreateRandomValidQuote());
  do {
    packed_quote.pop_back();
    EXPECT_THAT(ParseDcapPackedQuote(packed_quote),
                StatusIs(error::GoogleError::INVALID_ARGUMENT));
  } while (!packed_quote.empty());
}

TEST_F(IntelEcdsaQuoteTest, RoundTripPackUnpackPack) {
  auto packed_quote = PackDcapQuote(CreateRandomValidQuote());

  IntelQeQuote parsed_quote;
  ASYLO_ASSERT_OK_AND_ASSIGN(parsed_quote, ParseDcapPackedQuote(packed_quote));

  EXPECT_THAT(PackDcapQuote(parsed_quote), ElementsAreArray(packed_quote));
}

}  // namespace
}  // namespace sgx
}  // namespace asylo
