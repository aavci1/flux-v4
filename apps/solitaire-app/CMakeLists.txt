lambda_add_app(solitaire-app
  APP_NAME "Solitaire"
  BUNDLE_IDENTIFIER "io.lambda.solitaire"
  CATEGORY "public.app-category.card-games"
  ICON "${CMAKE_CURRENT_SOURCE_DIR}/icon.svg"
  COMMENT "Solitaire for Lambda Desktop"
  FONTS "${PROJECT_SOURCE_DIR}/resources/fonts/MaterialSymbolsRounded.ttf"
  SOURCES main.cpp)
