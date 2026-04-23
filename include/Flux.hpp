#pragma once

/// \file Flux.hpp
///
/// Primary umbrella include for Flux: application/window, core types, reactive primitives, and 2D
/// graphics (Canvas, styles). Prefer including specific `<Flux/...>` headers when you only need a
/// subset to reduce compile time.

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/ImageNode.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneRenderer.hpp>
#include <Flux/SceneGraph/TextNode.hpp>
