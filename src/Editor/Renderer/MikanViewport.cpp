#include "App.h"
#include "EditorObjectSystem.h"
#include "IEditorWindow.h"
#include "IMkGraphicsContext.h"
#include "MikanViewport.h"
#include "MikanCamera.h"
#include "MkScene.h"
#include "MkStateStack.h"
#include "MkStateModifiers.h"
#include "MathUtility.h"
#include "Colors.h"
#include "InputManager.h"

// -- GlViewport --
MikanViewport::MikanViewport(const class IEditorWindow* ownerWindow)
	: m_ownerWindow(ownerWindow)
	, m_backgroundColor(Colors::CornflowerBlue, 1.f)
{
	setViewport(glm::i32vec2(0, 0), getWindowSize());
	addCamera();
}

glm::i32vec2 MikanViewport::getWindowSize() const
{
	return glm::i32vec2((int)m_ownerWindow->getWidth(), (int)m_ownerWindow->getHeight());
}

void MikanViewport::setViewport(const glm::i32vec2& viewportOrigin, const glm::i32vec2& viewportSize)
{
	const glm::i32vec2 windowSize= getWindowSize();

	m_viewportOrigin= glm::max(glm::min(viewportOrigin, windowSize), glm::i32vec2(0, 0));
	m_viewportSize= glm::min((m_viewportOrigin + viewportSize), windowSize) - m_viewportOrigin;

	// Net valid until applyViewport
	m_renderOrigin= glm::i32vec2();
	m_renderSize= glm::i32vec2();

	// Keep the current camera's projection matched to the real viewport aspect ratio
	// (matters for orthographic bounds and ray picking). Guard for the constructor
	// call, which runs before any camera exists.
	if (m_viewportSize.y > 0 && getCameraCount() > 0)
	{
		MikanCameraPtr camera= getCurrentMikanCamera();
		if (camera)
		{
			camera->setViewportAspect((float)m_viewportSize.x / (float)m_viewportSize.y);
		}
	}
}

void MikanViewport::setBackgroundColor(const glm::vec3& color) { m_backgroundColor= glm::vec4(color, 1.f); }

MikanViewport::~MikanViewport() { unbindInput(); }

void MikanViewport::applyRenderingViewport(IMkState* glState)
{
	// Register this viewport on the graphics context BEFORE mkStateSetViewport is
	// called, so that mkStateSetViewportImpl::apply() can find us via
	// getOwnerContext()->getRenderingViewport() and call onRenderingViewportApply.
	m_ownerWindow->getGraphicsContext()->setRenderingViewport(shared_from_this());
	m_appliedRenderRects.clear();

	mkStateSetClearColor(glState, m_backgroundColor);

	// This calls onRenderingViewportApply from mkStateSetViewportImpl.
	// onRenderingViewportRevert is called when the scoped state is popped.
	// The window height converts the top-left origin the dockspace works in to
	// the bottom-left one GL wants, so it has to be the live height.
	mkStateSetViewport(glState, m_viewportOrigin.x, getWindowSize().y - (m_viewportOrigin.y + m_viewportSize.y),
					   m_viewportSize.x, m_viewportSize.y);
}

void MikanViewport::onRenderingViewportApply(int x, int y, int width, int height)
{
	m_appliedRenderRects.push_back(glm::i32vec4(x, y, width, height));
	m_renderOrigin= glm::i32vec2(x, y);
	m_renderSize= glm::i32vec2(width, height);
}

void MikanViewport::onRenderingViewportRevert(int x, int y, int width, int height)
{
	if (!m_appliedRenderRects.empty())
	{
		m_appliedRenderRects.pop_back();
	}

	if (!m_appliedRenderRects.empty())
	{
		// A nested rect (a frame buffer bound mid-render) was popped: the outer one is current again
		const glm::i32vec4& outerRect= m_appliedRenderRects.back();
		m_renderOrigin= glm::i32vec2(outerRect.x, outerRect.y);
		m_renderSize= glm::i32vec2(outerRect.z, outerRect.w);
	}
	else
	{
		m_renderOrigin= glm::i32vec2(x, y);
		m_renderSize= glm::i32vec2(width, height);

		// Deregister from the graphics context when this viewport's own scoped state is popped
		m_ownerWindow->getGraphicsContext()->setRenderingViewport(nullptr);
	}
}

bool MikanViewport::getRenderingViewport(glm::i32vec2& outOrigin, glm::i32vec2& outSize) const
{
	if (m_renderSize.x > 0 && m_renderSize.y > 0)
	{
		outOrigin= m_renderOrigin;
		outSize= m_renderSize;
		return true;
	}

	return false;
}

void MikanViewport::update(float deltaSeconds)
{
	// Don't process input if the cursor isn't in the viewport
	glm::vec2 viewportLocation;
	if (!getCursorViewportPixelPos(viewportLocation))
		return;

	MikanCameraPtr camera= std::static_pointer_cast<MikanCamera>(getCurrentCamera());
	if (!camera)
		return;

	if (camera->getCameraMovementMode() == fly)
	{
		auto editorObjectSystem= m_ownerWindow->getProjectManager()->getSystemOfType<EditorObjectSystem>();
		const float cameraSpeed= editorObjectSystem->getEditorSystemConfig()->getCameraSpeed();
		const float moveDelta= cameraSpeed * deltaSeconds;

		if (m_isLeftPressed || m_isRightPressed)
		{
			const float leftDelta= (m_isLeftPressed) ? -moveDelta : 0.f;
			const float rightDelta= (m_isRightPressed) ? moveDelta : 0.f;

			camera->adjustFlyRight(leftDelta + rightDelta);
		}

		if (m_isForwardPressed || m_isBackwardPressed)
		{
			const float forwardDelta= (m_isForwardPressed) ? moveDelta : 0.f;
			const float backwardDelta= (m_isBackwardPressed) ? -moveDelta : 0.f;

			camera->adjustFlyForward(forwardDelta + backwardDelta);
		}

		if (m_isUpPressed || m_isDownPressed)
		{
			const float upDelta= (m_isUpPressed) ? moveDelta : 0.f;
			const float downDelta= (m_isDownPressed) ? -moveDelta : 0.f;

			camera->adjustFlyUp(upDelta + downDelta);
		}
	}
}

IMkCameraPtr MikanViewport::getCurrentCamera() const { return m_cameraPool[m_currentCameraIndex]; }

int MikanViewport::getCurrentCameraIndex() const { return m_currentCameraIndex; }

IMkCameraPtr MikanViewport::addCamera()
{
	MikanCameraPtr newCamera= std::make_shared<MikanCamera>();
	m_cameraPool.push_back(newCamera);

	return newCamera;
}

int MikanViewport::getCameraCount() const { return (int)m_cameraPool.size(); }

IMkCameraPtr MikanViewport::getCameraByIndex(int cameraIndex)
{
	if (cameraIndex >= 0 && cameraIndex < getCameraCount())
	{
		return m_cameraPool[cameraIndex];
	}

	return nullptr;
}

bool MikanViewport::removeCameraByIndex(int cameraIndex)
{
	if (cameraIndex >= 0 && cameraIndex < getCameraCount())
	{
		m_cameraPool.erase(m_cameraPool.begin() + cameraIndex);
		if (m_currentCameraIndex >= getCameraCount())
		{
			m_currentCameraIndex= getCameraCount() - 1;
		}
		return true;
	}

	return false;
}

void MikanViewport::setCurrentCamera(int cameraIndex)
{
	if (cameraIndex >= 0 && cameraIndex < getCameraCount())
	{
		m_currentCameraIndex= cameraIndex;
	}
}

void MikanViewport::setCurrentCamera(IMkCameraPtr camera)
{
	for (int i= 0; i < getCameraCount(); ++i)
	{
		if (m_cameraPool[i] == camera)
		{
			m_currentCameraIndex= i;
			return;
		}
	}
}

MikanCameraPtr MikanViewport::getCurrentMikanCamera() const
{
	return std::static_pointer_cast<MikanCamera>(getCurrentCamera());
}

MikanCameraPtr MikanViewport::addMikanCamera() { return std::static_pointer_cast<MikanCamera>(addCamera()); }

MikanCameraPtr MikanViewport::getMikanCameraByIndex(int cameraIndex)
{
	return std::static_pointer_cast<MikanCamera>(getCameraByIndex(cameraIndex));
}

void MikanViewport::bindInput()
{
	if (!m_bIsInputBound)
	{
		InputManager* inputManager= m_ownerWindow->getInputManager();
		EventBindingSet* bindingSet= inputManager->getCurrentEventBindingSet();

		bindingSet->OnMouseButtonPressedEvent+= MakeDelegate(this, &MikanViewport::onMouseButtonPressed);
		bindingSet->OnMouseButtonReleasedEvent+= MakeDelegate(this, &MikanViewport::onMouseButtonReleased);
		bindingSet->OnMouseMotionEvent+= MakeDelegate(this, &MikanViewport::onMouseMotion);
		bindingSet->OnMouseWheelScrolledEvent+= MakeDelegate(this, &MikanViewport::onMouseWheel);

		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_a)->OnKeyPressed+=
			MakeDelegate(this, &MikanViewport::onLeftButtonPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_a)->OnKeyReleased+=
			MakeDelegate(this, &MikanViewport::onLeftButtonReleased);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_d)->OnKeyPressed+=
			MakeDelegate(this, &MikanViewport::onRightButtonPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_d)->OnKeyReleased+=
			MakeDelegate(this, &MikanViewport::onRightButtonReleased);

		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_w)->OnKeyPressed+=
			MakeDelegate(this, &MikanViewport::onForwardButtonPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_w)->OnKeyReleased+=
			MakeDelegate(this, &MikanViewport::onForwardButtonReleased);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_s)->OnKeyPressed+=
			MakeDelegate(this, &MikanViewport::onBackwardButtonPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_s)->OnKeyReleased+=
			MakeDelegate(this, &MikanViewport::onBackwardButtonReleased);

		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_e)->OnKeyPressed+=
			MakeDelegate(this, &MikanViewport::onUpButtonPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_e)->OnKeyReleased+=
			MakeDelegate(this, &MikanViewport::onUpButtonReleased);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_q)->OnKeyPressed+=
			MakeDelegate(this, &MikanViewport::onDownButtonPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_q)->OnKeyReleased+=
			MakeDelegate(this, &MikanViewport::onDownButtonReleased);

		inputManager->fetchOrAddKeyBindings(MkKey::LEFT_CTRL)->OnKeyPressed+=
			MakeDelegate(this, &MikanViewport::onLeftCtrlPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LEFT_CTRL)->OnKeyReleased+=
			MakeDelegate(this, &MikanViewport::onLeftCtrlReleased);
		inputManager->fetchOrAddKeyBindings(MkKey::RIGHT_CTRL)->OnKeyPressed+=
			MakeDelegate(this, &MikanViewport::onRightCtrlPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::RIGHT_CTRL)->OnKeyReleased+=
			MakeDelegate(this, &MikanViewport::onRightCtrlReleased);

		inputManager->fetchOrAddKeyBindings(MkKey::LEFT_ALT)->OnKeyPressed+=
			MakeDelegate(this, &MikanViewport::onLeftAltPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LEFT_ALT)->OnKeyReleased+=
			MakeDelegate(this, &MikanViewport::onLeftAltReleased);
		inputManager->fetchOrAddKeyBindings(MkKey::RIGHT_ALT)->OnKeyPressed+=
			MakeDelegate(this, &MikanViewport::onRightAltPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::RIGHT_ALT)->OnKeyReleased+=
			MakeDelegate(this, &MikanViewport::onRightAltReleased);

		inputManager->fetchOrAddKeyBindings(MkKey::LEFT_SHIFT)->OnKeyPressed+=
			MakeDelegate(this, &MikanViewport::onLeftShiftPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LEFT_SHIFT)->OnKeyReleased+=
			MakeDelegate(this, &MikanViewport::onLeftShiftReleased);
		inputManager->fetchOrAddKeyBindings(MkKey::RIGHT_SHIFT)->OnKeyPressed+=
			MakeDelegate(this, &MikanViewport::onRightShiftPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::RIGHT_SHIFT)->OnKeyReleased+=
			MakeDelegate(this, &MikanViewport::onRightShiftReleased);

		m_bIsInputBound= true;
	}
}

void MikanViewport::unbindInput()
{
	if (m_bIsInputBound)
	{
		InputManager* inputManager= m_ownerWindow->getInputManager();
		EventBindingSet* bindingSet= inputManager->getCurrentEventBindingSet();

		bindingSet->OnMouseButtonPressedEvent-= MakeDelegate(this, &MikanViewport::onMouseButtonPressed);
		bindingSet->OnMouseButtonReleasedEvent-= MakeDelegate(this, &MikanViewport::onMouseButtonReleased);
		bindingSet->OnMouseMotionEvent-= MakeDelegate(this, &MikanViewport::onMouseMotion);
		bindingSet->OnMouseWheelScrolledEvent-= MakeDelegate(this, &MikanViewport::onMouseWheel);

		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_a)->OnKeyPressed-=
			MakeDelegate(this, &MikanViewport::onLeftButtonPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_a)->OnKeyReleased-=
			MakeDelegate(this, &MikanViewport::onLeftButtonReleased);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_d)->OnKeyPressed-=
			MakeDelegate(this, &MikanViewport::onRightButtonPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_d)->OnKeyReleased-=
			MakeDelegate(this, &MikanViewport::onRightButtonReleased);

		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_w)->OnKeyPressed-=
			MakeDelegate(this, &MikanViewport::onForwardButtonPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_w)->OnKeyReleased-=
			MakeDelegate(this, &MikanViewport::onForwardButtonReleased);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_s)->OnKeyPressed-=
			MakeDelegate(this, &MikanViewport::onBackwardButtonPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_s)->OnKeyReleased-=
			MakeDelegate(this, &MikanViewport::onBackwardButtonReleased);

		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_e)->OnKeyPressed-=
			MakeDelegate(this, &MikanViewport::onUpButtonPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_e)->OnKeyReleased-=
			MakeDelegate(this, &MikanViewport::onUpButtonReleased);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_q)->OnKeyPressed-=
			MakeDelegate(this, &MikanViewport::onDownButtonPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_q)->OnKeyReleased-=
			MakeDelegate(this, &MikanViewport::onDownButtonReleased);

		inputManager->fetchOrAddKeyBindings(MkKey::LEFT_CTRL)->OnKeyPressed-=
			MakeDelegate(this, &MikanViewport::onLeftCtrlPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LEFT_CTRL)->OnKeyReleased-=
			MakeDelegate(this, &MikanViewport::onLeftCtrlReleased);
		inputManager->fetchOrAddKeyBindings(MkKey::RIGHT_CTRL)->OnKeyPressed-=
			MakeDelegate(this, &MikanViewport::onRightCtrlPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::RIGHT_CTRL)->OnKeyReleased-=
			MakeDelegate(this, &MikanViewport::onRightCtrlReleased);

		inputManager->fetchOrAddKeyBindings(MkKey::LEFT_ALT)->OnKeyPressed-=
			MakeDelegate(this, &MikanViewport::onLeftAltPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LEFT_ALT)->OnKeyReleased-=
			MakeDelegate(this, &MikanViewport::onLeftAltReleased);
		inputManager->fetchOrAddKeyBindings(MkKey::RIGHT_ALT)->OnKeyPressed-=
			MakeDelegate(this, &MikanViewport::onRightAltPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::RIGHT_ALT)->OnKeyReleased-=
			MakeDelegate(this, &MikanViewport::onRightAltReleased);

		inputManager->fetchOrAddKeyBindings(MkKey::LEFT_SHIFT)->OnKeyPressed-=
			MakeDelegate(this, &MikanViewport::onLeftShiftPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LEFT_SHIFT)->OnKeyReleased-=
			MakeDelegate(this, &MikanViewport::onLeftShiftReleased);
		inputManager->fetchOrAddKeyBindings(MkKey::RIGHT_SHIFT)->OnKeyPressed-=
			MakeDelegate(this, &MikanViewport::onRightShiftPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::RIGHT_SHIFT)->OnKeyReleased-=
			MakeDelegate(this, &MikanViewport::onRightShiftReleased);

		m_bIsInputBound= false;
	}
}

bool MikanViewport::getCursorViewportPixelPos(glm::vec2& outViewportLocation) const
{
	int mouse_x, mouse_y;
	m_ownerWindow->getInputManager()->getMouseScreenPosition(mouse_x, mouse_y);

	const int min_x= m_viewportOrigin.x;
	const int min_y= m_viewportOrigin.y;
	const int max_x= min_x + m_viewportSize.x;
	const int max_y= min_y + m_viewportSize.y;

	if (mouse_x >= min_x && mouse_x <= max_x && mouse_y >= min_y && mouse_y <= max_y)
	{
		outViewportLocation.x= (float)mouse_x - (float)min_x;
		outViewportLocation.y= (float)mouse_y - (float)min_y;
		return true;
	}

	return false;
}

void MikanViewport::onMouseMotion(int deltaX, int deltaY)
{
	MikanCameraPtr camera= std::static_pointer_cast<MikanCamera>(getCurrentCamera());

	if (camera)
	{
		// Compute the relative cursor position in the viewport
		glm::vec2 viewportPos;
		if (getCursorViewportPixelPos(viewportPos))
		{
			// See if we just entered the viewport
			if (!m_isMouseInViewport)
			{
				m_isMouseInViewport= true;
				if (OnMouseEntered)
					OnMouseEntered();
			}

			// Compute the camera ray through the cursor's pixel position
			glm::vec3 rayOrigin, rayDir;
			camera->computeCameraRayThruPixel(shared_from_this(), viewportPos, rayOrigin, rayDir);

			// Broadcast to any viewport raycast listeners
			if (OnMouseRayChanged)
				OnMouseRayChanged(rayOrigin, rayDir);
		}

		if (m_isCameraRotateButtonPressed)
		{
			if (camera->isOrthographic())
			{
				// In orthographic mode there is no rotation; right-drag pans the view.
				const float viewportHeight= (float)m_viewportSize.y;
				const float worldPerPixel=
					(viewportHeight > 0.f) ? (2.f * camera->getOrthoExtent() / viewportHeight) : 0.f;
				const glm::vec3 right= camera->getCameraRightFromViewMatrix();
				const glm::vec3 up= camera->getCameraUpFromViewMatrix();

				// Grab-and-drag: move the target opposite the cursor's horizontal motion,
				// and with the cursor's vertical motion (screen Y grows downward).
				const glm::vec3 delta= (-(float)deltaX * right + (float)deltaY * up) * worldPerPixel;
				camera->adjustOrthoTargetPosition(delta);
			}
			else
			{
				float deltaYaw= -(float)deltaX * k_camera_mouse_pan_scalar;
				float deltaPitch= (float)deltaY * k_camera_mouse_pan_scalar;

				switch (camera->getCameraMovementMode())
				{
				case eCameraMovementMode::fly:
				{
					if (!is_nearly_zero(deltaYaw))
						camera->adjustFlyYaw(deltaYaw);

					if (!is_nearly_zero(deltaPitch))
						camera->adjustFlyPitch(deltaPitch);
				}
				break;
				case eCameraMovementMode::orbit:
				{
					camera->adjustOrbitAngles(deltaYaw, deltaPitch);
				}
				break;
				default:
					break;
				}
			}
		}
	}
	else
	{
		if (m_isMouseInViewport)
		{
			m_isMouseInViewport= false;
			if (OnMouseExited)
				OnMouseExited();
		}
	}
}

void MikanViewport::onMouseButtonPressed(int button)
{
	MikanCameraPtr camera= std::static_pointer_cast<MikanCamera>(getCurrentCamera());

	glm::vec2 viewportPos;
	if (camera && getCursorViewportPixelPos(viewportPos))
	{
		glm::vec3 rayOrigin, rayDir;
		camera->computeCameraRayThruPixel(shared_from_this(), viewportPos, rayOrigin, rayDir);

		// Broadcast to any viewport raycast listeners
		if (OnMouseRayButtonDown)
			OnMouseRayButtonDown(rayOrigin, rayDir, button);

		if (button == MkMouseButton::RIGHT)
		{
			m_isCameraRotateButtonPressed= true;
		}
	}
}

void MikanViewport::onMouseButtonReleased(int button)
{
	MikanCameraPtr camera= std::static_pointer_cast<MikanCamera>(getCurrentCamera());

	if (camera)
	{
		glm::vec2 viewportPos;
		if (getCursorViewportPixelPos(viewportPos))
		{
			glm::vec3 rayOrigin, rayDir;
			camera->computeCameraRayThruPixel(shared_from_this(), viewportPos, rayOrigin, rayDir);

			// Broadcast to any viewport raycast listeners
			if (OnMouseRayButtonUp)
				OnMouseRayButtonUp(rayOrigin, rayDir, button);
		}

		if (button == MkMouseButton::RIGHT)
		{
			m_isCameraRotateButtonPressed= false;
		}
	}
}

void MikanViewport::onMouseWheel(int scrollAmount)
{
	MikanCameraPtr camera= std::static_pointer_cast<MikanCamera>(getCurrentCamera());
	if (!camera)
		return;

	if (camera->isOrthographic())
	{
		// Scrolling up zooms in (shrinks the visible extent), scrolling down zooms out.
		const float zoomFactor= fmaxf(0.1f, 1.f - (float)scrollAmount * k_camera_mouse_zoom_scalar);
		camera->setOrthoExtent(camera->getOrthoExtent() * zoomFactor);
	}
	else
	{
		const float deltaRadius= (float)scrollAmount * k_camera_mouse_zoom_scalar;
		camera->adjustOrbitRadius(deltaRadius);
	}
}