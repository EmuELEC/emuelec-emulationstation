#include "GridTileComponent.h"

#include "resources/TextureResource.h"
#include "ThemeData.h"

#include <algorithm>

#include "animations/LambdaAnimation.h"
#include "ImageIO.h"

#ifdef _RPI_
#include "components/VideoPlayerComponent.h"
#endif
#include "components/VideoVlcComponent.h"
#include "utils/FileSystemUtil.h"

#include "Settings.h"
#include "ImageGridComponent.h"

#define VIDEODELAY	100

GridTileComponent::GridTileComponent(Window* window) : GuiComponent(window), mBackground(window), mLabel(window), mVideo(nullptr), mVideoPlaying(false), mShown(false)
{	
	mSelectedZoomPercent = 1.0f;
	mAnimPosition = Vector3f(0, 0);
	mVideo = nullptr;
	mMarquee = nullptr;

	mLabelVisible = false;
	mLabelMerged = false;

	resetProperties();

	mImage = std::make_shared<ImageComponent>(mWindow);
	mImage->setOrigin(0.5f, 0.5f);

	addChild(&mBackground);
	addChild(&(*mImage));
	addChild(&mLabel);

	setSelected(false);
	setVisible(true);
}

void GridTileComponent::resetProperties()
{
	mDefaultProperties.mSize = getDefaultTileSize();
	mDefaultProperties.mPadding = Vector2f(16.0f, 16.0f);
	mDefaultProperties.mImageColor = 0xFFFFFFDD; // 0xAAAAAABB;
	mDefaultProperties.mBackgroundImage = ":/frame.png";
	mDefaultProperties.mBackgroundCornerSize = Vector2f(16, 16);
	mDefaultProperties.mBackgroundCenterColor = 0xAAAAEEFF;
	mDefaultProperties.mBackgroundEdgeColor = 0xAAAAEEFF;

	mSelectedProperties.mSize = getSelectedTileSize();
	mSelectedProperties.mPadding = mDefaultProperties.mPadding;
	mSelectedProperties.mImageColor = 0xFFFFFFFF;
	mSelectedProperties.mBackgroundImage = mDefaultProperties.mBackgroundImage;
	mSelectedProperties.mBackgroundCornerSize = mDefaultProperties.mBackgroundCornerSize;
	mSelectedProperties.mBackgroundCenterColor = 0xFFFFFFFF;
	mSelectedProperties.mBackgroundEdgeColor = 0xFFFFFFFF;

	mDefaultProperties.mLabelPos = Vector2f(-1, -1);
	mDefaultProperties.mLabelSize = Vector2f(1.0, 0.30);
	mDefaultProperties.mLabelColor = 0xFFFFFFFF;
	mDefaultProperties.mLabelBackColor = 0;
	mDefaultProperties.mLabelGlowColor = 0;
	mDefaultProperties.mLabelGlowSize = 2;

	mDefaultProperties.mFontPath = "";
	mDefaultProperties.mFontSize = 0;

	mSelectedProperties.mLabelPos = Vector2f(-1, -1);
	mSelectedProperties.mLabelSize = Vector2f(1.0, 0.30);
	mSelectedProperties.mLabelColor = 0xFFFFFFFF;
	mSelectedProperties.mLabelBackColor = 0;
	mSelectedProperties.mLabelGlowColor = 0;
	mSelectedProperties.mLabelGlowSize = 2;

	mSelectedProperties.mFontPath = "";
	mSelectedProperties.mFontSize = 0;

	mDefaultProperties.mMirror = Vector2f(0, 0);
	mSelectedProperties.mMirror = Vector2f(0, 0);
}

void GridTileComponent::forceSize(Vector2f size, float selectedZoom)
{
	mDefaultProperties.mSize = size;
	mSelectedProperties.mSize = size * selectedZoom;
}

GridTileComponent::~GridTileComponent()
{
	if (mVideo != nullptr)
		delete mVideo;

	mVideo = nullptr;
}

std::shared_ptr<TextureResource> GridTileComponent::getTexture(bool marquee) 
{ 
	if (marquee && mMarquee  != nullptr)
		return mMarquee->getTexture();
	else if (!marquee && mImage != nullptr)
		return mImage->getTexture();

	return nullptr; 
}

void GridTileComponent::resize()
{
	auto currentProperties = getCurrentProperties();

	Vector2f size = currentProperties.mSize;
	if (mSize != size)
		setSize(size);

	float height = (int) (size.y() * currentProperties.mLabelSize.y());
	float labelHeight = height;


	if (mLabelVisible)
	{
		mLabel.setColor(currentProperties.mLabelColor);
		mLabel.setBackgroundColor(currentProperties.mLabelBackColor);
		mLabel.setGlowColor(currentProperties.mLabelGlowColor);
		mLabel.setGlowSize(currentProperties.mLabelGlowSize);

		if (mDefaultProperties.mFontPath != mSelectedProperties.mFontPath || mDefaultProperties.mFontSize != mSelectedProperties.mFontSize)
			mLabel.setFont(currentProperties.mFontPath, currentProperties.mFontSize);

		if (currentProperties.mLabelPos.x() < 0)
		{
			if (mLabelMerged)
			{
				mLabel.setPosition(currentProperties.mPadding.x(), mSize.y() - height - currentProperties.mPadding.y());
				mLabel.setSize(size.x() - 2 * currentProperties.mPadding.x(), height);
			}
			else
			{
				mLabel.setPosition(0, mSize.y() - height);
				mLabel.setSize(size.x(), height);
			}
		}
	}

	if (!mLabelVisible || mLabelMerged || currentProperties.mLabelSize.x() == 0)
		height = 0;

	float topPadding = currentProperties.mPadding.y();
	float bottomPadding = std::max(topPadding, height);
	float paddingX = currentProperties.mPadding.x();
	
	float imageWidth = size.x() - paddingX * 2.0;
	float imageHeight = size.y() - topPadding - bottomPadding;

	if (mMarquee != nullptr)
	{		
		mMarquee->setPosition(
			currentProperties.mMarqueePos.x() * size.x(),
			currentProperties.mMarqueePos.y() * size.y());

		mMarquee->setMaxSize(
			currentProperties.mMarqueeSize.x() * size.x(), 
			currentProperties.mMarqueeSize.y() * size.y());
	}
	
	if (mImage != nullptr)
	{		
		mImage->setPosition(size.x() / 2.0f, (size.y() - height) / 2.0f);
		mImage->setColorShift(currentProperties.mImageColor);
		mImage->setMirroring(currentProperties.mMirror);

		if (currentProperties.mImageSizeMode == "minSize")
			mImage->setMinSize(imageWidth, imageHeight);
		else if (currentProperties.mImageSizeMode == "size")
			mImage->setSize(imageWidth, imageHeight);
		else
			mImage->setMaxSize(imageWidth, imageHeight);

		if (mLabelVisible && currentProperties.mLabelPos.x() < 0)
		{
			if (mLabelMerged)
			{
				mLabel.setPosition(mImage->getPosition().x() - mImage->getSize().x() / 2, currentProperties.mPadding.y() + mImage->getSize().y() - labelHeight);
				mLabel.setSize(mImage->getSize().x(), labelHeight);
			}
			else if (currentProperties.mPadding.x() == 0)
			{
				mLabel.setPosition(mImage->getPosition().x() - mImage->getSize().x() / 2, mImage->getSize().y());
				mLabel.setSize(mImage->getSize().x(), labelHeight);
			}
		}
	}

	if (currentProperties.mLabelPos.x() >= 0)
	{
		mLabel.setPosition(
			currentProperties.mLabelPos.x() * size.x(),
			currentProperties.mLabelPos.y() * size.y());

		mLabel.setSize(
			currentProperties.mLabelSize.x() * size.x(),
			currentProperties.mLabelSize.y() * size.y());
	}

	if (mVideo != nullptr && mVideo->isPlaying())
	{
		mVideo->setPosition(size.x() / 2.0f, (size.y() - height) / 2.0f);

		if (currentProperties.mImageSizeMode == "minSize")
		{
			auto vs = mVideo->getVideoSize();
			if (vs == Vector2f(0, 0))
				vs = Vector2f(640, 480);

			mVideo->setSize(ImageIO::getPictureMinSize(vs, Vector2f(imageWidth, imageHeight)));
		}
		else 
		if (currentProperties.mImageSizeMode == "size")
			mVideo->setSize(imageWidth, size.y() - topPadding - bottomPadding );
		else
			mVideo->setMaxSize(imageWidth, size.y() - topPadding - bottomPadding);
	}

	Vector3f bkposition = Vector3f(0, 0);
	Vector2f bkSize = size;

	if (mImage != NULL && currentProperties.mSelectionMode == "image" && mImage->getSize() != Vector2f(0,0))
	{
		if (currentProperties.mImageSizeMode == "minSize")
			bkSize = Vector2f(size.x(), size.y() - bottomPadding + topPadding);
		else if (mAnimPosition == Vector3f(0, 0, 0))
		{
			bkposition = Vector3f(
				mImage->getPosition().x() - mImage->getSize().x() / 2 - mSelectedProperties.mPadding.x(),
				mImage->getPosition().y() - mImage->getSize().y() / 2 - mSelectedProperties.mPadding.y(), 0);

			bkSize = Vector2f(mImage->getSize().x() + 2 * mSelectedProperties.mPadding.x(), mImage->getSize().y() + 2 * mSelectedProperties.mPadding.y());
		}
		else
		{
			bkposition = Vector3f(
				mImage->getPosition().x() - mImage->getSize().x() / 2 - currentProperties.mPadding.x(),
				mImage->getPosition().y() - mImage->getSize().y() / 2 - currentProperties.mPadding.y(), 0);

			bkSize = Vector2f(mImage->getSize().x() + 2 * currentProperties.mPadding.x(), mImage->getSize().y() + 2 * currentProperties.mPadding.y());
		}
	}

	if (mSelectedZoomPercent != 1.0f && mAnimPosition.x() != 0 && mAnimPosition.y() != 0 && mSelected)
	{
		float x = mPosition.x() + bkposition.x();
		float y = mPosition.y() + bkposition.y();

		x = mAnimPosition.x() * (1.0 - mSelectedZoomPercent) + x * mSelectedZoomPercent;
		y = mAnimPosition.y() * (1.0 - mSelectedZoomPercent) + y * mSelectedZoomPercent;

		bkposition = Vector3f(x - mPosition.x(), y - mPosition.y(), 0);
	}

	mBackground.setPosition(bkposition);
	mBackground.setSize(bkSize);
	mBackground.setCornerSize(currentProperties.mBackgroundCornerSize);
	mBackground.setCenterColor(currentProperties.mBackgroundCenterColor);
	mBackground.setEdgeColor(currentProperties.mBackgroundEdgeColor);
	mBackground.setImagePath(currentProperties.mBackgroundImage);
	
	if (mSelected && mAnimPosition == Vector3f(0, 0, 0) && mSelectedZoomPercent != 1.0)
		mBackground.setOpacity(mSelectedZoomPercent * 255);
	else
		mBackground.setOpacity(255);
}

void GridTileComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);
	
	if (mVideo != nullptr && mVideo->isPlaying())
		resize();
}

void GridTileComponent::renderBackground(const Transform4x4f& parentTrans)
{
	if (!mVisible)
		return;

	Transform4x4f trans = getTransform() * parentTrans;
	mBackground.render(trans);
}

void GridTileComponent::renderContent(const Transform4x4f& parentTrans)
{
	if (!mVisible)
		return;

	Transform4x4f trans = getTransform() * parentTrans;

	Vector2f clipPos(trans.translation().x(), trans.translation().y());
	if (!Renderer::isVisibleOnScreen(clipPos.x(), clipPos.y(), mSize.x(), mSize.y()))
		return;

	auto currentProperties = getCurrentProperties();

	float padding = currentProperties.mPadding.x();
	float topPadding = currentProperties.mPadding.y();
	float bottomPadding = topPadding;

	if (mLabelVisible && !mLabelMerged)
		bottomPadding = std::max((int)topPadding, (int)(mSize.y() * currentProperties.mLabelSize.y()));

	Vector2i pos((int)Math::round(trans.translation()[0] + padding), (int)Math::round(trans.translation()[1] + topPadding));
	Vector2i size((int)Math::round(mSize.x() - 2 * padding), (int)Math::round(mSize.y() - topPadding - bottomPadding));
	
	if (currentProperties.mImageSizeMode == "minSize")
		Renderer::pushClipRect(pos, size);

	if (mImage != NULL)
		mImage->render(trans);

	if (mSelected && !mVideoPath.empty() && mVideo != nullptr)
		mVideo->render(trans);

	if (!mLabelMerged && currentProperties.mImageSizeMode == "minSize")
		Renderer::popClipRect();

	if (mMarquee != nullptr && mMarquee->hasImage())
		mMarquee->render(trans);
	else if (mLabelVisible && currentProperties.mLabelSize.y()>0)
		mLabel.render(trans);

	if (mLabelMerged && currentProperties.mImageSizeMode == "minSize")
		Renderer::popClipRect();
}

void GridTileComponent::render(const Transform4x4f& parentTrans)
{
	if (!mVisible)
		return;

	renderBackground(parentTrans);
	renderContent(parentTrans);
}

void GridTileComponent::createMarquee()
{
	if (mMarquee != nullptr)
		return;

	mMarquee = new ImageComponent(mWindow);
	mMarquee->setOrigin(0.5f, 0.5f);
	mMarquee->setDefaultZIndex(35);
	addChild(mMarquee);
}

void GridTileComponent::createVideo()
{
	if (mVideo != nullptr)
		return;

	mVideo = new VideoVlcComponent(mWindow, "");

	// video
	mVideo->setOrigin(0.5f, 0.5f);
	mVideo->setStartDelay(VIDEODELAY);
	mVideo->setDefaultZIndex(30);
	addChild(mVideo);
}

void GridTileComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	resetProperties();

	Vector2f screen = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	const ThemeData::ThemeElement* grid = theme->getElement(view, "gamegrid", "imagegrid");
	if (grid && grid->has("showVideoAtDelay"))
	{
		createVideo();
		mVideo->applyTheme(theme, view, element, properties);
	}
	else if (mVideo != nullptr)
	{
		removeChild(mVideo);
		delete mVideo;
		mVideo = nullptr;
	}
	
	// Apply theme to the default gridtile
	const ThemeData::ThemeElement* elem = theme->getElement(view, "default", "gridtile");
	if (elem)
	{		
		if (elem->has("size"))
			mDefaultProperties.mSize = elem->get<Vector2f>("size") * screen;

		if (elem->has("padding"))
			mDefaultProperties.mPadding = elem->get<Vector2f>("padding");

		if (elem->has("imageColor"))
			mDefaultProperties.mImageColor = elem->get<unsigned int>("imageColor");

		if (elem->has("backgroundImage"))
			mDefaultProperties.mBackgroundImage = elem->get<std::string>("backgroundImage");

		if (elem->has("backgroundCornerSize"))
			mDefaultProperties.mBackgroundCornerSize = elem->get<Vector2f>("backgroundCornerSize");

		if (elem->has("backgroundColor"))
		{
			mDefaultProperties.mBackgroundCenterColor = elem->get<unsigned int>("backgroundColor");
			mDefaultProperties.mBackgroundEdgeColor = elem->get<unsigned int>("backgroundColor");
		}

		if (elem->has("backgroundCenterColor"))
			mDefaultProperties.mBackgroundCenterColor = elem->get<unsigned int>("backgroundCenterColor");

		if (elem->has("backgroundEdgeColor"))
			mDefaultProperties.mBackgroundEdgeColor = elem->get<unsigned int>("backgroundEdgeColor");

		if (elem && elem->has("imageSizeMode"))
		{
			mDefaultProperties.mImageSizeMode = elem->get<std::string>("imageSizeMode");
			mSelectedProperties.mImageSizeMode = mDefaultProperties.mImageSizeMode;
		}

		if (elem && elem->has("selectionMode"))
		{
			mDefaultProperties.mSelectionMode = elem->get<std::string>("selectionMode");
			mSelectedProperties.mSelectionMode = mDefaultProperties.mSelectionMode;
		}

		if (elem && elem->has("reflexion"))
		{
			mDefaultProperties.mMirror = elem->get<Vector2f>("reflexion");
			mSelectedProperties.mMirror = mDefaultProperties.mMirror;
		}
	}
	
	elem = theme->getElement(view, "gridtile.marquee", "image");
	if (elem)
	{
		createMarquee();
		mMarquee->applyTheme(theme, view, "gridtile.marquee", ThemeFlags::ALL ^ (ThemeFlags::PATH));

		if (elem->has("pos"))
			mDefaultProperties.mMarqueePos = elem->get<Vector2f>("pos");
		else 
			mDefaultProperties.mMarqueeSize = Vector2f(0, 0);

		if (elem->has("pos"))
			mDefaultProperties.mMarqueeSize = elem->get<Vector2f>("size");
		else
			mDefaultProperties.mMarqueeSize = Vector2f(1, 1);

		mSelectedProperties.mMarqueePos = mDefaultProperties.mMarqueePos;
		mSelectedProperties.mMarqueeSize = mDefaultProperties.mMarqueeSize;
	}

	else if (mMarquee != nullptr)
	{
		removeChild(mMarquee);
		delete mMarquee;
		mMarquee = nullptr;
	}

	// Apply theme to the selected gridtile
	// NOTE that some of the default gridtile properties influence on the selected gridtile properties
	// See THEMES.md for more informations
	elem = theme->getElement(view, "selected", "gridtile");

	mSelectedProperties.mSize = elem && elem->has("size") ?
								elem->get<Vector2f>("size") * screen :
								mDefaultProperties.mSize;
								//getSelectedTileSize();

	mSelectedProperties.mPadding = elem && elem->has("padding") ?
								   elem->get<Vector2f>("padding") :
								   mDefaultProperties.mPadding;

	if (elem && elem->has("imageColor"))
		mSelectedProperties.mImageColor = elem->get<unsigned int>("imageColor");

	mSelectedProperties.mBackgroundImage = elem && elem->has("backgroundImage") ?
										   elem->get<std::string>("backgroundImage") :
										   mDefaultProperties.mBackgroundImage;

	mSelectedProperties.mBackgroundCornerSize = elem && elem->has("backgroundCornerSize") ?
												elem->get<Vector2f>("backgroundCornerSize") :
												mDefaultProperties.mBackgroundCornerSize;

	if (elem && elem->has("backgroundColor"))
	{
		mSelectedProperties.mBackgroundCenterColor = elem->get<unsigned int>("backgroundColor");
		mSelectedProperties.mBackgroundEdgeColor = elem->get<unsigned int>("backgroundColor");
	}

	if (elem && elem->has("backgroundCenterColor"))
		mSelectedProperties.mBackgroundCenterColor = elem->get<unsigned int>("backgroundCenterColor");

	if (elem && elem->has("backgroundEdgeColor"))
		mSelectedProperties.mBackgroundEdgeColor = elem->get<unsigned int>("backgroundEdgeColor");

	if (elem && elem->has("reflexion"))
		mSelectedProperties.mMirror = elem->get<Vector2f>("reflexion");

	elem = theme->getElement(view, "gridtile", "text");
	if (elem != NULL)
	{
		float sh = (float)Renderer::getScreenHeight();

		if (elem->has("pos"))
		{
			mDefaultProperties.mLabelPos = elem->get<Vector2f>("pos");
			mSelectedProperties.mLabelPos = mDefaultProperties.mLabelPos;
			mLabelMerged = true;
		}
		else 
			mLabelMerged = false;

		if (elem->has("size"))
		{
			mDefaultProperties.mLabelSize = elem->get<Vector2f>("size");
			mSelectedProperties.mLabelSize = mDefaultProperties.mLabelSize;

			if (!mLabelMerged)
				mLabelMerged = mDefaultProperties.mLabelSize.x() == 0;
		}

		if (elem->has("color"))
		{
			mDefaultProperties.mLabelColor = elem->get<unsigned int>("color");
			mSelectedProperties.mLabelColor = mDefaultProperties.mLabelColor;			
		}

		if (elem->has("backgroundColor"))
		{
			mDefaultProperties.mLabelBackColor = elem->get<unsigned int>("backgroundColor");
			mSelectedProperties.mLabelBackColor = mDefaultProperties.mLabelBackColor;			
		}

		if (elem->has("glowSize"))
		{
			mDefaultProperties.mLabelGlowSize = (unsigned int) elem->get<float>("glowSize");
			mSelectedProperties.mLabelGlowSize = mDefaultProperties.mLabelGlowSize;
		}

		if (elem->has("glowColor"))
		{
			mDefaultProperties.mLabelGlowColor = elem->get<unsigned int>("glowColor");
			mSelectedProperties.mLabelGlowColor = mDefaultProperties.mLabelGlowColor;
		}

		if (elem->has("fontSize"))
		{
			mDefaultProperties.mFontSize = elem->get<float>("fontSize") * sh;
			mSelectedProperties.mFontSize = mDefaultProperties.mFontSize;
		}

		if (elem->has("fontPath"))
		{
			mDefaultProperties.mFontPath = elem->get<std::string>("fontPath");
			mSelectedProperties.mFontPath = mDefaultProperties.mFontPath;
		}
	
		if (elem->has("visible"))
			mLabelVisible = elem->get<bool>("visible");
		else
			mLabelVisible = true;

		mLabel.applyTheme(theme, view, element, properties);

		elem = theme->getElement(view, "gridtile_selected", "text");
		if (elem != NULL)
		{
			if (elem->has("pos") && mDefaultProperties.mLabelPos.x() >= 0)
				mSelectedProperties.mLabelPos = elem->get<Vector2f>("pos");

			if (elem->has("size"))
				mSelectedProperties.mLabelSize = elem->get<Vector2f>("size");

			if (elem->has("color"))
				mSelectedProperties.mLabelColor = elem->get<unsigned int>("color");

			if (elem->has("backgroundColor"))
				mSelectedProperties.mLabelBackColor = elem->get<unsigned int>("backgroundColor");

			if (elem->has("glowSize"))
				mSelectedProperties.mLabelGlowSize = (unsigned int)elem->get<float>("glowSize");

			if (elem->has("glowColor"))
				mSelectedProperties.mLabelGlowColor = elem->get<unsigned int>("glowColor");

			if (elem->has("fontSize"))
				mSelectedProperties.mFontSize = elem->get<float>("fontSize") * sh;

			if (elem->has("fontPath"))
				mSelectedProperties.mFontPath = elem->get<std::string>("fontPath");
		}
	}
	else
		mLabelVisible = false;
}

// Made this a static function because the ImageGridComponent need to know the default tile size
// to calculate the grid dimension before it instantiate the GridTileComponents
Vector2f GridTileComponent::getDefaultTileSize()
{
	Vector2f screen = Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	return screen * 0.22f;
}

Vector2f GridTileComponent::getSelectedTileSize() const
{
	return mDefaultProperties.mSize * 1.2f;
}

bool GridTileComponent::isSelected() const
{
	return mSelected;
}

void GridTileComponent::setImage(const std::string& path)
{
	if (mCurrentPath == path)
		return;
	
	mCurrentPath = path;		

	if (mSelectedProperties.mSize.x() > mSize.x())
		mImage->setImage(path, false, MaxSizeInfo(mSelectedProperties.mSize, mSelectedProperties.mImageSizeMode != "maxSize"));
	else
		mImage->setImage(path, false, MaxSizeInfo(mSize, mSelectedProperties.mImageSizeMode != "maxSize"));

	resize();	
}

void GridTileComponent::setMarquee(const std::string& path)
{
	if (mMarquee == nullptr)
		return;

	if (mCurrentMarquee == path)
		return;

	mCurrentMarquee = path;

	if (mSelectedProperties.mSize.x() > mSize.x())
		mMarquee->setImage(path, false, MaxSizeInfo(mSelectedProperties.mSize));
	else
		mMarquee->setImage(path, false, MaxSizeInfo(mSize));

	resize();
}

void GridTileComponent::resetImages()
{
	setLabel("");	
	setImage("");
	setMarquee("");
	stopVideo();
}

void GridTileComponent::setLabel(std::string name)
{
	if (mLabel.getText() == name)
		return;

	mLabel.setText(name);
	resize();
}

void GridTileComponent::setVideo(const std::string& path, float defaultDelay)
{
	if (mVideoPath == path)
		return;

	mVideoPath = path;

	if (mVideo != nullptr)
	{
		if (defaultDelay >= 0.0)
			mVideo->setStartDelay(defaultDelay);
		
		if (mVideoPath.empty())
			stopVideo();
	}

	resize();
}

void GridTileComponent::onShow()
{
	GuiComponent::onShow();
	mShown = true;
}

void GridTileComponent::onHide()
{
	GuiComponent::onHide();
	mShown = false;
}

void GridTileComponent::startVideo()
{
	if (mVideo != nullptr)
	{		
		// Inform video component about size before staring in order to be able to use OptimizeVideo parameter
		if (mSelectedProperties.mImageSizeMode == "minSize")
			mVideo->setMinSize(mSelectedProperties.mSize);
		else 
			mVideo->setResize(mSelectedProperties.mSize);
		
		mVideo->setVideo(mVideoPath);
	}
}

void GridTileComponent::stopVideo()
{
	if (mVideo != nullptr)
		mVideo->setVideo("");
}

void GridTileComponent::setSelected(bool selected, bool allowAnimation, Vector3f* pPosition, bool force)
{
	if (!mShown || !ALLOWANIMATIONS)
		allowAnimation = false;

	if (mSelected == selected && !force)
	{
		if (mSelected)
			startVideo();

		return;
	}

	mSelected = selected;	

	if (!mSelected)
		stopVideo();

	if (selected)
	{
		if (pPosition == NULL || !allowAnimation)
		{			
			cancelAnimation(3);
			
			this->setSelectedZoom(1);
			mAnimPosition = Vector3f(0, 0, 0);
			startVideo();

			resize();
		}
		else
		{
			if (pPosition == NULL)
				mAnimPosition = Vector3f(0, 0, 0);
			else
				mAnimPosition = Vector3f(pPosition->x(), pPosition->y(), pPosition->z());

			auto func = [this](float t)
			{
				t -= 1; // cubic ease out
				float pct = Math::lerp(0, 1, t*t*t + 1);

				this->setSelectedZoom(pct);
			};

			cancelAnimation(3);
			setAnimation(new LambdaAnimation(func, 250), 0, [this] {
				this->setSelectedZoom(1);
				mAnimPosition = Vector3f(0, 0, 0);
				startVideo();
			}, false, 3);
		}
	}
	else // if (!selected)
	{
		if (!allowAnimation)
		{
			cancelAnimation(3);
			this->setSelectedZoom(0);
			stopVideo();
			resize();
		}
		else
		{
			this->setSelectedZoom(1);
			stopVideo();

			auto func = [this](float t)
			{
				t -= 1; // cubic ease out
				float pct = Math::lerp(0, 1, t*t*t + 1);
				this->setSelectedZoom(1.0 - pct);
			};
			
			cancelAnimation(3);
			setAnimation(new LambdaAnimation(func, 250), 0, [this] {
				this->setSelectedZoom(0);
			}, false, 3);
		}
	}
}

void GridTileComponent::setSelectedZoom(float percent)
{
	if (mSelectedZoomPercent == percent)
		return;

	mSelectedZoomPercent = percent;
	resize();
}

void GridTileComponent::setVisible(bool visible)
{
	mVisible = visible;
}

GridTileProperties GridTileComponent::getCurrentProperties() 
{
	GridTileProperties mMixedProperties;

	if (mSelectedZoomPercent == 0.0f || mSelectedZoomPercent == 1.0f)
		return mSelected ? mSelectedProperties : mDefaultProperties;

	auto def = mSelected ? mSelectedProperties : mDefaultProperties;

	mMixedProperties = mSelected ? mSelectedProperties : mDefaultProperties;

	if (mDefaultProperties.mSize != mSelectedProperties.mSize)
	{
		float x = mDefaultProperties.mSize.x() * (1.0 - mSelectedZoomPercent) + mSelectedProperties.mSize.x() * mSelectedZoomPercent;
		float y = mDefaultProperties.mSize.y() * (1.0 - mSelectedZoomPercent) + mSelectedProperties.mSize.y() * mSelectedZoomPercent;
		mMixedProperties.mSize = Vector2f(x, y);
	}

	if (mDefaultProperties.mPadding != mSelectedProperties.mPadding)
	{
		float x = mDefaultProperties.mPadding.x() * (1.0 - mSelectedZoomPercent) + mSelectedProperties.mPadding.x() * mSelectedZoomPercent;
		float y = mDefaultProperties.mPadding.y() * (1.0 - mSelectedZoomPercent) + mSelectedProperties.mPadding.y() * mSelectedZoomPercent;
		mMixedProperties.mPadding = Vector2f(x, y);
	}

	if (mDefaultProperties.mImageColor != mSelectedProperties.mImageColor)
	{
		mMixedProperties.mImageColor = Renderer::mixColors(mDefaultProperties.mImageColor, mSelectedProperties.mImageColor, mSelectedZoomPercent);
	}

	if (mDefaultProperties.mLabelSize != mSelectedProperties.mLabelSize)
		mMixedProperties.mLabelSize = Vector2f(mDefaultProperties.mLabelSize.x(), 
			mDefaultProperties.mLabelSize.y() * (1.0 - mSelectedZoomPercent) + mSelectedProperties.mLabelSize.y() * mSelectedZoomPercent);

	if (mDefaultProperties.mLabelColor != mSelectedProperties.mLabelColor)
		mMixedProperties.mLabelColor = Renderer::mixColors(mDefaultProperties.mLabelColor, mSelectedProperties.mLabelColor, mSelectedZoomPercent);

	if (mDefaultProperties.mLabelBackColor != mSelectedProperties.mLabelBackColor)
		mMixedProperties.mLabelBackColor = Renderer::mixColors(mDefaultProperties.mLabelBackColor, mSelectedProperties.mLabelBackColor, mSelectedZoomPercent);

	if (mDefaultProperties.mLabelGlowColor != mSelectedProperties.mLabelGlowColor)
		mMixedProperties.mLabelGlowColor = Renderer::mixColors(mDefaultProperties.mLabelGlowColor, mSelectedProperties.mLabelGlowColor, mSelectedZoomPercent);

	if (mDefaultProperties.mLabelGlowSize != mSelectedProperties.mLabelGlowSize)
		mMixedProperties.mLabelGlowSize = mDefaultProperties.mLabelGlowSize * (1.0 - mSelectedZoomPercent) + mSelectedProperties.mLabelGlowSize * mSelectedZoomPercent;
	
	if (mDefaultProperties.mMirror != mSelectedProperties.mMirror)
	{
		float x = mDefaultProperties.mMirror.x() * (1.0 - mSelectedZoomPercent) + mSelectedProperties.mMirror.x() * mSelectedZoomPercent;
		float y = mDefaultProperties.mMirror.y() * (1.0 - mSelectedZoomPercent) + mSelectedProperties.mMirror.y() * mSelectedZoomPercent;
		mMixedProperties.mMirror = Vector2f(x, y);
	}

//  Avoid to multiply font sizes in mem + it create strange sizings
//	if (mDefaultProperties.mFontSize != mSelectedProperties.mFontSize)
	//	mMixedProperties.mFontSize = mDefaultProperties.mFontSize * (1.0 - mSelectedZoomPercent) + mSelectedProperties.mFontSize * mSelectedZoomPercent;

	return mMixedProperties;
}

Vector3f GridTileComponent::getBackgroundPosition() 
{ 
	return Vector3f(mBackground.getPosition().x() + mPosition.x(), mBackground.getPosition().y() + mPosition.y(), 0);
}