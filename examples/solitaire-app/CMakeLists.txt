flux_add_app(solitaire-app ../solitaire-demo/main.cpp
  APP_NAME "Solitaire"
  BUNDLE_IDENTIFIER "com.aavci1.solitaire"
  VERSION "1.0"
  BUILD "1"
  COPYRIGHT "Copyright (c) 2026 Abdurrahman Avci"
  CATEGORY "public.app-category.card-games"
  MIN_MACOS "12.0"
  FONTS "${PROJECT_SOURCE_DIR}/resources/fonts/MaterialSymbolsRounded.ttf")

target_compile_definitions(solitaire-app PRIVATE FLUX_SOLITAIRE_APP=1)
