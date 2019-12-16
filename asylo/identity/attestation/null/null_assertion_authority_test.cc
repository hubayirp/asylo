/*
 *
 * Copyright 2017 Asylo authors
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

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "asylo/identity/attestation/enclave_assertion_generator.h"
#include "asylo/identity/attestation/enclave_assertion_verifier.h"
#include "asylo/identity/attestation/null/internal/null_identity_constants.h"
#include "asylo/identity/attestation/null/null_assertion_generator.h"
#include "asylo/identity/attestation/null/null_assertion_verifier.h"
#include "asylo/identity/descriptions.h"
#include "asylo/identity/enclave_assertion_authority.h"
#include "asylo/identity/enclave_assertion_authority_config.pb.h"
#include "asylo/identity/identity.pb.h"
#include "asylo/identity/init.h"
#include "asylo/test/util/enclave_assertion_authority_configs.h"
#include "asylo/test/util/status_matchers.h"

// The unit tests in this file test compatibility of NullAssertionGenerator and
// NullAssertionVerifier by checking the following:
//   * NullAssertionGenerator can generate assertions to satisfy assertion
//     requests from a NullAssertionVerifier.
//   * NullAssertionGenerator can generate assertions based on assertion
//     requests from NullAssertionVerifier.
//   * NullAssertionVerifier can verify assertions offered by a
//     NullAssertionGenerator.
//   * NullAssertionVerifier can verify assertions generated by
//     NullAssertionGenerator.

namespace asylo {
namespace {

using ::testing::Not;

// Placeholder values for the handshake transcript and a Diffie-Hellman public
// key.
const char kEkepContext[] = "EKEP handshake transcript and public key";

// Values that are invalid in null assertions, null assertion offers, and
// null assertion requests.
const char kInvalidAuthorityType[] = "SGX Local";
const char kInvalidAssertionOfferAdditionalInfo[] = "offer info";
const char kInvalidAssertionRequestAdditionalInfo[] = "request info";
const char kInvalidAssertion[] = "assertion";

// A test fixture is used to store pointers to instances of
// NullAssertionGenerator and NullAssertionVerifier, which are retrieved from
// the AssertionGeneratorMap and AssertionVerifierMap.
class NullAssertionAuthorityTest : public ::testing::Test {
 protected:
  void SetUp() override {
    AssertionDescription null_assertion_description;
    SetNullAssertionDescription(&null_assertion_description);

    // The same key is used for NullAssertionGenerator in AssertionGeneratorMap
    // and for NullAssertionVerifier in AssertionVerifierMap.
    std::string map_key = EnclaveAssertionAuthority::GenerateAuthorityId(
                              null_assertion_description.identity_type(),
                              null_assertion_description.authority_type())
                              .ValueOrDie();

    auto generator = AssertionGeneratorMap::GetValue(map_key);
    ASSERT_NE(generator, AssertionGeneratorMap::value_end());

    auto verifier = AssertionVerifierMap::GetValue(map_key);
    ASSERT_NE(verifier, AssertionVerifierMap::value_end());

    std::vector<EnclaveAssertionAuthorityConfig> authority_configs = {
      GetNullAssertionAuthorityTestConfig()
    };

    // Explicitly initialize the null assertion authorities.
    ASSERT_THAT(InitializeEnclaveAssertionAuthorities(
                    authority_configs.cbegin(), authority_configs.cend()),
                IsOk());

    // Tests in this file only need to call const methods of
    // NullAssertionGenerator and NullAssertionVerifier.
    // Note that, since a static-map is stable at this point (i.e., no
    // additions/deletions can happen at this point), it is safe to store
    // pointers to objects stored in the static map for long-term use.
    generator_ = &*generator;
    verifier_ = &*verifier;
  }

  // An instance of NullAssertionGenerator.
  const EnclaveAssertionGenerator *generator_;

  // An instance of NullAssertionVerifier.
  const EnclaveAssertionVerifier *verifier_;
};

// Verify that an assertion offered by a NullAssertionGenerator is verifiable by
// a NullAssertionVerifier.
TEST_F(NullAssertionAuthorityTest, CanVerifySuceedsVerifyAssertionOffer) {
  AssertionOffer offer;
  ASSERT_THAT(generator_->CreateAssertionOffer(&offer), IsOk());

  EXPECT_THAT(verifier_->CanVerify(offer), IsOkAndHolds(true));
}

// Verify that neither an empty assertion offer nor an invalid assertion offer
// is verifiable by a NullAssertionVerifier.
TEST_F(NullAssertionAuthorityTest, CanVerifyFailsBadAssertionOffer) {
  // Invalid assertion offer.
  AssertionOffer offer;
  offer.mutable_description()->set_identity_type(
      EnclaveIdentityType::CODE_IDENTITY);
  offer.mutable_description()->set_authority_type(kInvalidAuthorityType);
  offer.set_additional_information(kInvalidAssertionOfferAdditionalInfo);

  EXPECT_THAT(verifier_->CanVerify(offer), IsOkAndHolds(false));

  // Empty assertion offer.
  offer.Clear();
  EXPECT_THAT(verifier_->CanVerify(offer), IsOkAndHolds(false));
}

// Verify that a NullAssertionGenerator can generate an assertion to satisfy an
// assertion request from a NullAssertionVerifier.
TEST_F(NullAssertionAuthorityTest, CanGenerateSuceedsValidAssertionRequest) {
  AssertionRequest request;
  ASSERT_THAT(verifier_->CreateAssertionRequest(&request), IsOk());

  EXPECT_THAT(generator_->CanGenerate(request), IsOkAndHolds(true));
}

// Verify that a NullAssertionGenerator cannot generate an assertion to satisfy
// an invalid assertion request or an empty assertion request.
TEST_F(NullAssertionAuthorityTest, CanGenerateFailsBadAssertionRequest) {
  // Invalid assertion request.
  AssertionRequest request;
  request.mutable_description()->set_identity_type(
      EnclaveIdentityType::CODE_IDENTITY);
  request.mutable_description()->set_authority_type(kInvalidAuthorityType);
  request.set_additional_information(kInvalidAssertionRequestAdditionalInfo);

  EXPECT_THAT(generator_->CanGenerate(request), IsOkAndHolds(false));

  // Empty assertion request.
  request.Clear();
  EXPECT_THAT(generator_->CanGenerate(request), IsOkAndHolds(false));
}

// Verify that a NullAssertionGenerator can generate an assertion based on an
// assertion request created by a NullAssertionVerifier.
TEST_F(NullAssertionAuthorityTest, GenerateSuceedsValidAssertionRequest) {
  AssertionRequest request;
  ASSERT_THAT(verifier_->CreateAssertionRequest(&request), IsOk());

  Assertion assertion;
  EXPECT_THAT(generator_->Generate(kEkepContext, request, &assertion), IsOk());
}

// Verify that a NullAssertionGenerator cannot generate an assertion based on an
// invalid assertion request or an empty assertion request.
TEST_F(NullAssertionAuthorityTest, GenerateFailsBadAssertionRequest) {
  Assertion assertion;

  // Invalid assertion request.
  AssertionRequest request;
  request.mutable_description()->set_identity_type(
      EnclaveIdentityType::CODE_IDENTITY);
  request.mutable_description()->set_authority_type(kInvalidAuthorityType);
  request.set_additional_information(kInvalidAssertionRequestAdditionalInfo);

  EXPECT_THAT(generator_->Generate(kEkepContext, request, &assertion),
              Not(IsOk()));

  // Empty assertion request.
  request.Clear();
  EXPECT_THAT(generator_->Generate(kEkepContext, request, &assertion),
              Not(IsOk()));
}

// Verify that a NullAssertionVerifier can verify an assertion generated by a
// NullAssertionGenerator, and that the extracted identity is as expected.
TEST_F(NullAssertionAuthorityTest, VerifySuceedsValidAssertion) {
  AssertionRequest request;
  ASSERT_THAT(verifier_->CreateAssertionRequest(&request), IsOk());

  Assertion assertion;
  ASSERT_THAT(generator_->Generate(kEkepContext, request, &assertion), IsOk());

  EnclaveIdentity peer_identity;
  ASSERT_THAT(verifier_->Verify(kEkepContext, assertion, &peer_identity),
              IsOk());
  EXPECT_EQ(peer_identity.description().identity_type(),
            EnclaveIdentityType::NULL_IDENTITY);
  EXPECT_EQ(peer_identity.description().authority_type(),
            kNullAuthorizationAuthority);
  EXPECT_EQ(peer_identity.identity(), kNullIdentity);
}

// Verify that a NullAssertionVerifier cannot verify an empty assertion.
TEST_F(NullAssertionAuthorityTest, VerifyFailsEmptyAssertion) {
  EnclaveIdentity peer_identity;

  // Empty assertion.
  EXPECT_THAT(verifier_->Verify(kEkepContext, {}, &peer_identity), Not(IsOk()));
}

// Verify that a NullAssertionVerifier cannot verify an assertion with an
// invalid assertion description.
TEST_F(NullAssertionAuthorityTest, VerifyFailsBadAssertionRequest) {
  EnclaveIdentity peer_identity;

  // Assertion with invalid assertion description.
  Assertion assertion;
  assertion.mutable_description()->set_identity_type(
      EnclaveIdentityType::CODE_IDENTITY);
  assertion.mutable_description()->set_authority_type(kInvalidAuthorityType);
  EXPECT_THAT(verifier_->Verify(kEkepContext, assertion, &peer_identity),
              Not(IsOk()));
}

// Verify that a NullAssertionVerifier cannot verify an assertion with an
// invalid assertion body.
TEST_F(NullAssertionAuthorityTest, VerifyFailsBadAssertion) {
  EnclaveIdentity peer_identity;

  // Assertion with an invalid assertion body.
  Assertion assertion;
  assertion.mutable_description()->set_identity_type(
      EnclaveIdentityType::NULL_IDENTITY);
  assertion.mutable_description()->set_authority_type(kNullAssertionAuthority);
  assertion.set_assertion(kInvalidAssertion);
  EXPECT_THAT(verifier_->Verify(kEkepContext, assertion, &peer_identity),
              Not(IsOk()));
}

}  // namespace
}  // namespace asylo