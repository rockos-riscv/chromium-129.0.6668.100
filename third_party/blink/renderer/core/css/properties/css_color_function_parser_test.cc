// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_color_function_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_relative_color_value.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

TEST(ColorFunctionParserTest, RelativeColorWithKeywordBase) {
  const String test_case = "rgb(from red r g b)";
  CSSTokenizer tokenizer(test_case);
  CSSParserTokenStream stream(tokenizer);

  const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  ColorFunctionParser parser;
  const CSSValue* result =
      parser.ConsumeFunctionalSyntaxColor(stream, *context);
  EXPECT_TRUE(result->IsColorValue());
  const cssvalue::CSSColor* color = To<cssvalue::CSSColor>(result);
  EXPECT_EQ(color->Value(),
            Color::FromColorSpace(Color::ColorSpace::kSRGB, 1, 0, 0));
}

TEST(ColorFunctionParserTest, RelativeColorWithInvalidChannelReference) {
  const String test_case = "rgb(from red h s l)";
  CSSTokenizer tokenizer(test_case);
  CSSParserTokenStream stream(tokenizer);

  const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  ColorFunctionParser parser;
  const CSSValue* result =
      parser.ConsumeFunctionalSyntaxColor(stream, *context);
  EXPECT_EQ(result, nullptr);
}

TEST(ColorFunctionParserTest, RelativeColorWithCurrentcolorBase_Disabled) {
  ScopedCSSRelativeColorSupportsCurrentcolorForTest scoped_feature_for_test(
      false);

  const String test_case = "rgb(from currentcolor r g b)";
  CSSTokenizer tokenizer(test_case);
  CSSParserTokenStream stream(tokenizer);

  const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  ColorFunctionParser parser;
  const CSSValue* result =
      parser.ConsumeFunctionalSyntaxColor(stream, *context);
  EXPECT_EQ(result, nullptr);
}

TEST(ColorFunctionParserTest, RelativeColorWithCurrentcolorBase_NoAlpha) {
  ScopedCSSRelativeColorSupportsCurrentcolorForTest scoped_feature_for_test(
      true);

  const String test_case = "rgb(from currentcolor 1 calc(g) b)";
  CSSTokenizer tokenizer(test_case);
  CSSParserTokenStream stream(tokenizer);

  const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  ColorFunctionParser parser;
  const CSSValue* result =
      parser.ConsumeFunctionalSyntaxColor(stream, *context);
  EXPECT_TRUE(result->IsRelativeColorValue());
  const cssvalue::CSSRelativeColorValue* color =
      To<cssvalue::CSSRelativeColorValue>(result);

  const CSSValue& origin = color->OriginColor();
  EXPECT_TRUE(origin.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(origin).GetValueID(),
            CSSValueID::kCurrentcolor);

  EXPECT_EQ(color->ColorInterpolationSpace(), Color::ColorSpace::kSRGBLegacy);

  const CSSValue& channel1 = color->Channel1();
  EXPECT_TRUE(channel1.IsNumericLiteralValue());
  EXPECT_EQ(To<CSSNumericLiteralValue>(channel1).DoubleValue(), 1.0f);

  const CSSValue& channel2 = color->Channel2();
  EXPECT_TRUE(channel2.IsMathFunctionValue());
  EXPECT_EQ(channel2.CssText(), "calc(g)");

  const CSSValue& channel3 = color->Channel3();
  EXPECT_TRUE(channel3.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(channel3).GetValueID(), CSSValueID::kB);

  EXPECT_EQ(color->Alpha(), nullptr);
}

TEST(ColorFunctionParserTest, RelativeColorWithCurrentcolorBase_CalcAlpha) {
  ScopedCSSRelativeColorSupportsCurrentcolorForTest scoped_feature_for_test(
      true);

  const String test_case =
      "rgb(from currentcolor 1 calc(g) b / calc(alpha / 2))";
  CSSTokenizer tokenizer(test_case);
  CSSParserTokenStream stream(tokenizer);

  const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  ColorFunctionParser parser;
  const CSSValue* result =
      parser.ConsumeFunctionalSyntaxColor(stream, *context);
  EXPECT_TRUE(result->IsRelativeColorValue());
  const cssvalue::CSSRelativeColorValue* color =
      To<cssvalue::CSSRelativeColorValue>(result);

  const CSSValue& origin = color->OriginColor();
  EXPECT_TRUE(origin.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(origin).GetValueID(),
            CSSValueID::kCurrentcolor);

  EXPECT_EQ(color->ColorInterpolationSpace(), Color::ColorSpace::kSRGBLegacy);

  const CSSValue& channel1 = color->Channel1();
  EXPECT_TRUE(channel1.IsNumericLiteralValue());
  EXPECT_EQ(To<CSSNumericLiteralValue>(channel1).DoubleValue(), 1.0f);

  const CSSValue& channel2 = color->Channel2();
  EXPECT_TRUE(channel2.IsMathFunctionValue());
  EXPECT_EQ(channel2.CssText(), "calc(g)");

  const CSSValue& channel3 = color->Channel3();
  EXPECT_TRUE(channel3.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(channel3).GetValueID(), CSSValueID::kB);

  const CSSValue* alpha = color->Alpha();
  EXPECT_TRUE(alpha->IsMathFunctionValue());
  EXPECT_EQ(alpha->CssText(), "calc(alpha / 2)");
}

TEST(ColorFunctionParserTest, RelativeColorWithCurrentcolorBase_NoneKeyword) {
  ScopedCSSRelativeColorSupportsCurrentcolorForTest scoped_feature_for_test(
      true);

  const String test_case = "rgb(from currentcolor none none none / none)";
  CSSTokenizer tokenizer(test_case);
  CSSParserTokenStream stream(tokenizer);

  const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  ColorFunctionParser parser;
  const CSSValue* result =
      parser.ConsumeFunctionalSyntaxColor(stream, *context);
  EXPECT_TRUE(result->IsRelativeColorValue());
  const cssvalue::CSSRelativeColorValue* color =
      To<cssvalue::CSSRelativeColorValue>(result);

  const CSSValue& origin = color->OriginColor();
  EXPECT_TRUE(origin.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(origin).GetValueID(),
            CSSValueID::kCurrentcolor);

  EXPECT_EQ(color->ColorInterpolationSpace(), Color::ColorSpace::kSRGBLegacy);

  const CSSValue& channel1 = color->Channel1();
  EXPECT_TRUE(channel1.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(channel1).GetValueID(), CSSValueID::kNone);

  const CSSValue& channel2 = color->Channel2();
  EXPECT_TRUE(channel2.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(channel2).GetValueID(), CSSValueID::kNone);

  const CSSValue& channel3 = color->Channel3();
  EXPECT_TRUE(channel3.IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(channel3).GetValueID(), CSSValueID::kNone);

  const CSSValue* alpha = color->Alpha();
  EXPECT_TRUE(alpha->IsIdentifierValue());
  EXPECT_EQ(To<CSSIdentifierValue>(alpha)->GetValueID(), CSSValueID::kNone);
}

}  // namespace blink
