#pragma once

#include <type_traits>
#include <utility>
#include <concepts>
#include <functional>
#include <vector>
#include "UIBuildMacros.hpp"

// Include GD and cocos2d classes before this


namespace uibuilder {
	using namespace cocos2d;
	using namespace cocos2d::extension;

	#ifdef GEODE_DLL
	using geode::Layout;
	template <typename F>
	using FunctionType = geode::Function<F>;
	#else
	template <typename F>
	using FunctionType = std::function<F>;
	#endif

	template <typename T> requires std::derived_from<T, CCObject>
	class Build;

	template <typename T>
	struct _remove_build {
		using value = std::remove_reference_t<std::remove_pointer_t<T>>;
	};

	template <typename T>
	struct _remove_build<Build<T>> {
		using value = T;
	};

	template <typename T>
	using remove_build_t = typename _remove_build<T>::value;

	template <typename T>
	struct remove_build {
		T operator()(T a) { return a; }
	};

	// helper classes

	class BuildSchedule : public CCNode {
		FunctionType<void(float)> m_callback;
	 public:
		inline static BuildSchedule* create(FunctionType<void(float)> cb) {
			auto bu = new BuildSchedule;

			bu->init();
			bu->autorelease();
			bu->m_callback = std::move(cb);
			return bu;
		}

		inline void onSchedule(float dt) {
			m_callback(dt);
		}
	};

	class BuildAction : public CCActionInstant {
		FunctionType<void(float)> m_callback;
	public:
		inline static BuildAction* create(FunctionType<void(float)> cb) {
			auto ba = new BuildAction;

			ba->autorelease();
			ba->m_callback = std::move(cb);
			return ba;
		}

		void update(float time) override {
			m_callback(time);
		}
	};


	// the thing

	inline std::vector<void*> buildStack;
	template <typename T> requires std::derived_from<T, CCObject>
	class Build {
		T* m_item;
	 public:

		Build<T> push() {
			buildStack.push_back(m_item);
			return *this;
		}

		static Build<T> pop() {
			auto ret = reinterpret_cast<T*>(buildStack.back());
			buildStack.pop_back();

			return Build<T>(ret);
		}

		static T* popRaw() {
			auto ret = reinterpret_cast<T*>(buildStack.back());
			buildStack.pop_back();

			return ret;
		}

		Build<T> store(T*& in) {
			in = m_item;
			return *this;
		}

		#ifdef GEODE_PLATFORM_TARGET
		Build<T> store(geode::Ref<T>& in) {
			in = m_item;
			return *this;
		}
		#endif

		template <typename ...Args> requires (!std::same_as<T, CCSequence> && requires(Args... args) {
			T::create(args...);
		})
		static Build<T> create(Args... args) {
			return Build(T::create(args...));
		}

		Build(T* item) : m_item(item) {}

		T* collect() const {return m_item;}

		template <typename U> requires std::derived_from<T, U>
		operator U*() { return m_item; }

		// Intellisense likes to throw a fit.
		#ifndef UIBUILDER_NO_ARROW
		T* operator->() { return m_item; }
		#endif

		// CCObject

		setter(CCObject, tag, setTag, int)

		// CCArray

		setter(CCArray, addItem, addObject, CCObject*);
		setter(CCArray, insertItemAt, insertObject, CCObject*, unsigned int)
		setter(CCArray, removeObject, removeObject, CCObject*)
		setter(CCArray, removeObjectAt, removeObjectAtIndex, unsigned int)
		setter(CCArray, reverse, reverseObjects)

		template <needs_base(CCArray), typename U>
		Build<U> intoItemAt(unsigned int index) {
			return Build<U>(m_item->objectAtIndex(index));
		}

		Build<T> with(FunctionType<void(T*)> fn) {
			fn(m_item);
			return *this;
		}

		template <typename U>
		Build<U> as() {
			return Build<U>(static_cast<U*>(m_item));
		}

		template <typename U, needs_base(CCArray)>
		Build<T> forEach(FunctionType<void(U*)> iter) {
			for (unsigned int i = 0; i < m_item->count(); ++i) {
				iter(static_cast<U*>(m_item->objectAtIndex(i)));
			}

			return *this;
		}

		// CCScheduler
		template <needs_same(CCScheduler)>
		static BuildSchedule* schedule(FunctionType<void(float)> fn, float interval = 0, unsigned int repeat = kCCRepeatForever) {
			auto node = BuildSchedule::create(std::move(fn));
			node->retain();

			CCDirector::sharedDirector()->getScheduler()->scheduleSelector(
				schedule_selector(BuildSchedule::onSchedule),
				node,
				interval,
				repeat,
				0,
				false
			);

			return node;
		}

		// CCNode
		setter(CCNode, pos, setPosition, CCPoint const&)
		setter(CCNode, posX, setPositionX, float)
		setter(CCNode, posY, setPositionY, float)
		setter(CCNode, scale, setScale, float)
		setter(CCNode, scaleX, setScaleX, float)
		setter(CCNode, scaleY, setScaleY, float)
		setter(CCNode, anchorPoint, setAnchorPoint, CCPoint const&)
		setter(CCNode, zOrder, setZOrder, int)
		setter(CCNode, skewX, setSkewX, float)
		setter(CCNode, skewY, setSkewY, float)
		setter(CCNode, contentSize, setContentSize, CCSize const&)
		setter(CCNode, visible, setVisible, bool)
		setter(CCNode, rotation, setRotation, float)
		setter(CCNode, ignoreAnchorPointForPos, ignoreAnchorPointForPosition, bool)
		setter(CCNode, child, addChild, CCNode*)
		setter(CCNode, userData, setUserData, void*)
		setter(CCNode, userObject, setUserObject, CCObject*)
		setter(CCNode, userObject, setUserObject, std::string const&, CCObject*)

		template <needs_base(CCNode), typename ...Args>
		Build<T> children(Args... chld) {
			(m_item->addChild(chld), ...);
			return *this;
		}

		template <needs_base(CCNode), typename U>
		Build<T> parent(U newParent) {
			remove_build<U>()(newParent)->addChild(m_item);
			return *this;
		}

		template <typename U = CCNode, needs_base(CCNode)>
		Build<U> intoParent() {
			return Build<U>(static_cast<U*>(m_item->getParent()));
		}

		template <typename U = CCNode, needs_base(CCNode)>
		Build<U> intoChild(unsigned int idx) {
			return Build<U>(static_cast<U*>(m_item->getChildren()->objectAtIndex(idx)));
		}

		template <typename U = CCNode, needs_base(CCNode)>
		Build<U> intoChildByTag(int tag) {
			return Build<U>(static_cast<U*>(m_item->getChildByTag(tag)));
		}

		template <needs_base(CCNode), typename U>
		Build<remove_build_t<U>> intoNewChild(U newChild, int idx = 0, int tag = 0) {
			m_item->addChild(remove_build<U>()(newChild), idx, tag);
			return Build<remove_build_t<U>>(newChild);
		}

		template <needs_base(CCNode), typename U>
		Build<remove_build_t<U>> intoNewSibling(U newSibling, int idx = 0, int tag = 0) {
			return intoParent().intoNewChild(newSibling, idx, tag);
		}

		template <needs_base(CCNode), typename U>
		Build<remove_build_t<U>> intoNewParent(U newParent) {
			remove_build<U>()(newParent)->addChild(m_item);
			return Build<remove_build_t<U>>(newParent);
		}

		template <needs_base(CCNode), typename U>
		Build<remove_build_t<U>> intoUserObject(std::string const& id) {
			auto obj = remove_build<U>()(m_item->getUserObject(id));
			return Build<remove_build_t<U>>(obj);
		}

		template <needs_base(CCNode)>
		Build<T> pos(float x, float y) {
			return pos(ccp(x, y));
		}

		template <needs_base(CCNode)>
		Build<T> contentSize(float w, float h) {
			return contentSize(CCSize(w, h));
		}

		template <needs_base(CCNode)>
		Build<T> width(float w) {
			return contentSize(w, m_item->getContentSize().height);
		}

		template <needs_base(CCNode)>
		Build<T> height(float h) {
			return contentSize(m_item->getContentSize().width, h);
		}

		template <needs_base(CCNode)>
		Build<T> scaleBy(float amt) {
			return scale(m_item->getScale() * amt);
		}

		template <needs_base(CCNode)>
		Build<T> scaleXBy(float amt) {
			return scaleX(m_item->getScaleX() * amt);
		}

		template <needs_base(CCNode)>
		Build<T> scaleYBy(float amt) {
			return scaleY(m_item->getScaleY() * amt);
		}

		template <needs_base(CCNode)>
		Build<T> scaleToMatchX(float x) {
			return scaleX(x / m_item->getContentSize().width);
		}

		template <needs_base(CCNode)>
		Build<T> scaleToMatchY(float y) {
			return scaleY(y / m_item->getContentSize().height);
		}

		template <needs_base(CCNode)>
		Build<T> scaleToMatch(CCSize size) {
			return scaleToMatchX(size.width).scaleToMatchY(size.height);
		}

		template <needs_base(CCNode)>
		Build<T> move(float x, float y) {
			return pos(m_item->getPositionX() + x, m_item->getPositionY() + y);
		}

		template <needs_base(CCNode)>
		Build<T> move(CCPoint const& p) {
			return move(p.x, p.y);
		}

		template <needs_base(CCNode)>
		Build<T> absoluteCenter() {
			return absolutePos(CCPoint(CCDirector::sharedDirector()->getWinSize() / 2.0));
		}

		template <needs_base(CCNode)>
		Build<T> center() {
			if (m_item->getParent() == nullptr)
				return pos(CCPoint(CCDirector::sharedDirector()->getWinSize() / 2.0));
			else
				return pos(m_item->getParent()->getContentSize() / 2.0);
		}

		template <needs_base(CCNode)>
		Build<T> centerX() {
			if (m_item->getParent() == nullptr)
				return posX(CCDirector::sharedDirector()->getWinSize().width / 2.0);
			else
				return posX(m_item->getParent()->getContentWidth() / 2.0);
		}

		template <needs_base(CCNode)>
		Build<T> centerY() {
			if (m_item->getParent() == nullptr)
				return posY(CCDirector::sharedDirector()->getWinSize().height / 2.0);
			else
				return posY(m_item->getParent()->getContentHeight() / 2.0);
		}

		template <needs_base(CCNode)>
		Build<T> absolutePos(CCPoint const& p) {
			if (m_item->getParent() == nullptr)
				return pos(p);
			else
				return pos(m_item->getParent()->convertToNodeSpace(p));
		}

		template <needs_base(CCNode)>
		Build<T> matchPos(CCNode* other) {
			if (other->getParent() != nullptr)
				return absolutePos(other->getParent()->convertToWorldSpace(other->getPosition()));
			else
				return absolutePos(other->getPosition());
		}

		template <needs_base(CCNode)>
		Build<T> anchorPoint(float x, float y) {
			return anchorPoint(ccp(x, y));
		}

		template <typename U, needs_base(CCNode), std::invocable<U*> F>
		Build<T> iterChildren(F iter) {
			for (unsigned int i = 0; i < m_item->getChildrenCount(); ++i) {
				iter(static_cast<U*>(m_item->getChildren()->objectAtIndex(i)));
			}

			return *this;
		}

		setter(CCNode, runAction, runAction, CCAction*)

		template <needs_base(CCNode)>
		Build<T> schedule(FunctionType<void(float)> fn, float interval = 0, int repeat = -1, float delay = 0) {
			auto node = BuildSchedule::create(std::move(fn));
			node->schedule(schedule_selector(BuildSchedule::onSchedule), interval, repeat, delay);
			m_item->addChild(node);
			return *this;
		}

		template <needs_base(CCNode)>
		Build<T> scheduleOnce(FunctionType<void(float)> fn, float delay = 0) {
			auto node = BuildSchedule::create(std::move(fn));
			node->scheduleOnce(schedule_selector(BuildSchedule::onSchedule), delay);
			m_item->addChild(node);
			return *this;
		}

		template <needs_base(CCNode)>
		Build<CCAction> intoAction(int tag) {
			return Build<CCAction>(m_item->getActionByTag(tag));
		}

		// Geode stuff
		#ifdef GEODE_DLL
		setter(CCNode, id, setID, std::string const&)
		setter(CCNode, layout, setLayout, geode::Layout*)
		setter(CCNode, layoutOpts, setLayoutOptions, geode::LayoutOptions*)

		template <needs_base(CCNode)>
		Build<T> updateLayout() {
			m_item->updateLayout();
			return *this;
		}

		template <typename U = CCNode, needs_base(CCNode)>
		Build<U> intoChildByID(std::string const& id) {
			return Build<U>(static_cast<U*>(m_item->getChildByID(id)));
		}

		template <needs_base(CCNode), typename U>
		Build<T> parentAtPos(U newParent, geode::Anchor anchor, CCPoint const& offset = CCPointZero, bool useAnchorLayout = true) {
			remove_build<U>()(newParent)->addChildAtPosition(m_item, anchor, offset, useAnchorLayout);
			return *this;
		}

		template <typename U = CCNode, needs_base(CCNode)>
		Build<U> intoChildRecurseID(std::string const& id) {
			return Build<U>(static_cast<U*>(m_item->getChildByIDRecursive(id)));
		}

		setter(geode::Layout, ignoreInvisible, ignoreInvisibleChildren, bool)

		setter(geode::AxisLayout, axis, setAxis, geode::Axis)
		setter(geode::AxisLayout, align, setAxisAlignment, geode::AxisAlignment)
		setter(geode::AxisLayout, crossAlign, setCrossAxisAlignment, geode::AxisAlignment)
		setter(geode::AxisLayout, lineAlign, setCrossAxisLineAlignment, geode::AxisAlignment)
		setter(geode::AxisLayout, gap, setGap, float)
		setter(geode::AxisLayout, reverse, setAxisReverse, bool)
		setter(geode::AxisLayout, crossReverse, setCrossAxisReverse, bool)
		setter(geode::AxisLayout, autoScale, setAutoScale, bool)
		setter(geode::AxisLayout, crossGrow, setGrowCrossAxis, bool)
		setter(geode::AxisLayout, crossOverflow, setCrossAxisOverflow, bool)
		setter(geode::AxisLayout, autoGrow, setAutoGrowAxis, std::optional<float>)

		setter(geode::AxisLayoutOptions, relativeScale, setRelativeScale, float)
		setter(geode::AxisLayoutOptions, autoScale, setAutoScale, std::optional<bool>)
		setter(geode::AxisLayoutOptions, length, setLength, std::optional<float>)
		setter(geode::AxisLayoutOptions, prevGap, setPrevGap, std::optional<float>)
		setter(geode::AxisLayoutOptions, nextGap, setNextGap, std::optional<float>)
		setter(geode::AxisLayoutOptions, breakLine, setBreakLine, bool)
		setter(geode::AxisLayoutOptions, sameLine, setSameLine, bool)
		setter(geode::AxisLayoutOptions, scalePrio, setScalePriority, int)
		setter(geode::AxisLayoutOptions, crossAlign, setCrossAxisAlignment, std::optional<geode::AxisAlignment>)

		setter(geode::AnchorLayoutOptions, anchor, setAnchor, geode::Anchor)
		setter(geode::AnchorLayoutOptions, offset, setOffset, CCPoint const&)

		template <needs_base(geode::AxisLayout)>
		static Build<T> createWithAligns(geode::AxisAlignment main, geode::AxisAlignment cross) {
			return Build<T>::create().align(main).crossAlign(cross);
		}

		template <needs_base(CCNode)>
		Build<T> setAnchorOpts(geode::Anchor anchor, CCPoint const& offset = CCPointZero) {
			auto opts = geode::AnchorLayoutOptions::create();
			opts->setAnchor(anchor);
			opts->setOffset(offset);
			m_item->setLayoutOptions(opts);
			return *this;
		}

		setter(geode::BasedButtonSprite, topScale, setTopRelativeScale, float)
		setter(geode::BasedButtonSprite, topOffset, setTopOffset, CCPoint const&)

		// geode::Button
		setter(geode::Button, enabled, setEnabled, bool)
		setter(geode::Button, displayNode, setDisplayNode, CCNode*)
		setter(geode::Button, animationType, setAnimationType, geode::Button::AnimationType)
		setter(geode::Button, activateCallback, setActivateCallback, geode::Button::ButtonCallback)
		setter(geode::Button, selectCallback, setSelectCallback, geode::Button::ButtonCallback)
		setter(geode::Button, unselectCallback, setUnselectCallback, geode::Button::ButtonCallback)
		setter(geode::Button, touchPriority, setTouchPriority, int)
		setter(geode::Button, touchMultiplier, setTouchMultiplier, float)
		setter(geode::Button, scaleMultiplier, setScaleMultiplier, float)
		setter(geode::Button, moveOffset, setMoveOffset, CCPoint const&)
		setter(geode::Button, selectedDuration, setSelectedDuration, float)
		setter(geode::Button, unselectedDuration, setUnselectedDuration, float)
		setter(geode::Button, swallowTouches, setSwallowTouches, bool)
		setter(geode::Button, dragCancelDistance, setDragCancelDistance, float)
		setter(geode::Button, clickAnimation, setClickAnimation, CCActionInterval*)
		setter(geode::Button, releaseAnimation, setReleaseAnimation, CCActionInterval*)

		template <needs_same(geode::Button), typename Callback>
		Build<T> addActivateCallback(geode::ZStringView id, Callback&& callback, int priority = 0) {
			m_item->addActivateCallback(id, std::forward<Callback>(callback), priority);
			return *this;
		}

		template <needs_same(geode::Button), typename Callback>
		Build<T> addSelectCallback(geode::ZStringView id, Callback&& callback, int priority = 0) {
			m_item->addSelectCallback(id, std::forward<Callback>(callback), priority);
			return *this;
		}

		template <needs_same(geode::Button), typename Callback>
		Build<T> addUnselectCallback(geode::ZStringView id, Callback&& callback, int priority = 0) {
			m_item->addUnselectCallback(id, std::forward<Callback>(callback), priority);
			return *this;
		}

		// geode::Toggler
		setter(geode::Toggler, offNode, setOffNode, CCNode*)
		setter(geode::Toggler, onNode, setOnNode, CCNode*)
		setter(geode::Toggler, toggleCallback, setToggleCallback, geode::Toggler::ToggleCallback)

		// geode::TextInput
		setter(geode::TextInput, placeholder, setPlaceholder, std::string)
		setter(geode::TextInput, placeholderColor, setPlaceholderColor, ccColor4B)
		setter(geode::TextInput, selectionColor, setSelectionColor, ccColor4B)
		setter(geode::TextInput, caretColor, setCaretColor, ccColor4B)
		setter(geode::TextInput, caretWidth, setCaretWidth, float)
		setter(geode::TextInput, textColor, setTextColor, ccColor4B)
		setter(geode::TextInput, padding, setPadding, CCPoint)
		setter(geode::TextInput, alignment, setAlignment, geode::Label::Alignment)
		setter(geode::TextInput, filter, setFilter, std::string)
		setter(geode::TextInput, commonFilter, setCommonFilter, geode::CommonFilter)
		setter(geode::TextInput, label, setLabel, std::string)
		setter(geode::TextInput, maxCharCount, setMaxCharCount, size_t)
		setter(geode::TextInput, passwordMode, setPasswordMode, bool)
		setter(geode::TextInput, numericMode, setNumericMode, bool)
		setter(geode::TextInput, multiline, setMultiline, bool)
		setter(geode::TextInput, enabled, setEnabled, bool)
		setter(geode::TextInput, wrap, setWrap, bool)
		setter(geode::TextInput, overflowMode, setOverflowMode, geode::TextInput::OverflowMode)
		setter(geode::TextInput, fontScale, setFontScale, float)
		setter(geode::TextInput, placeholderScale, setPlaceholderScale, float)
		setter(geode::TextInput, displayEmojis, setDisplayEmojis, bool)
		setter(geode::TextInput, hideBG, hideBG)
		setter(geode::TextInput, string, setString, std::string_view, bool)
		setter(geode::TextInput, callback, setCallback, geode::Function<void(std::string const&)>)
		setter(geode::TextInput, enterCallback, setEnterCallback, geode::Function<void(std::string const&)>)
		setter(geode::TextInput, focusCallback, setFocusCallback, geode::Function<void(geode::TextInput::FocusState)>)
		setter(geode::TextInput, keyboardOffsetCallback, setKeyboardOffsetCallback, geode::Function<void(float)>)
		setter(geode::TextInput, keyboardHideCallback, setKeyboardHideCallback, geode::Function<void()>)
		setter(geode::TextInput, cursorPos, setCursorPos, int)
		setter(geode::TextInput, selection, setSelection, int, int)

		template <needs_same(geode::TextInput), typename Callback>
		Build<T> addTextChangedCallback(geode::ZStringView id, Callback&& callback, int priority = 0) {
			m_item->addTextChangedCallback(id, std::forward<Callback>(callback), priority);
			return *this;
		}

		template <needs_same(geode::TextInput), typename Callback>
		Build<T> addFocusCallback(geode::ZStringView id, Callback&& callback, int priority = 0) {
			m_item->addFocusCallback(id, std::forward<Callback>(callback), priority);
			return *this;
		}

		template <needs_same(geode::TextInput), typename Callback>
		Build<T> addUnfocusCallback(geode::ZStringView id, Callback&& callback, int priority = 0) {
			m_item->addUnfocusCallback(id, std::forward<Callback>(callback), priority);
			return *this;
		}
		setter(geode::TextInput, focus, focus)
		setter(geode::TextInput, unfocus, unfocus)

		// geode::NineSlice
		setter(geode::NineSlice, insets, setInsets, geode::NineSlice::Insets const&)
		setter(geode::NineSlice, repeatCenter, setRepeatCenter, bool)
		setter(geode::NineSlice, scaleMultiplier, setScaleMultiplier, float)
		setter(geode::NineSlice, insetTop, setInsetTop, float)
		setter(geode::NineSlice, insetRight, setInsetRight, float)
		setter(geode::NineSlice, insetBottom, setInsetBottom, float)
		setter(geode::NineSlice, insetLeft, setInsetLeft, float)

		// geode::SliderNode
		setter(geode::SliderNode, value, setValue, float)
		setter(geode::SliderNode, percent, setPercent, float)
		setter(geode::SliderNode, min, setMin, float)
		setter(geode::SliderNode, max, setMax, float)
		setter(geode::SliderNode, enabled, setEnabled, bool)
		setter(geode::SliderNode, snapStep, setSnapStep, float)
		setter(geode::SliderNode, readOnly, setReadOnly, bool)
		setter(geode::SliderNode, sliderBypass, setSliderBypass, bool)
		setter(geode::SliderNode, slideCallback, setSlideCallback, geode::SliderNode::SliderCallback)
		setter(geode::SliderNode, clickCallback, setClickCallback, geode::SliderNode::SliderCallback)
		setter(geode::SliderNode, releaseCallback, setReleaseCallback, geode::SliderNode::SliderCallback)
		setter(geode::SliderNode, touchPriority, setTouchPriority, int)
		setter(geode::SliderNode, linkTextInput, linkTextInput, geode::TextInput*, unsigned int)
		setter(geode::SliderNode, linkLabel, linkLabel, geode::Label*, unsigned int)
		setter(geode::SliderNode, unlinkTextInput, unlinkTextInput)
		setter(geode::SliderNode, unlinkLabel, unlinkLabel)
		setter(geode::SliderNode, textInputPrecision, setTextInputPrecision, unsigned int)
		setter(geode::SliderNode, labelPrecision, setLabelPrecision, unsigned int)

		template <needs_same(geode::SliderNode), typename Callback>
		Build<T> addSlideCallback(geode::ZStringView id, Callback&& callback, int priority = 0) {
			m_item->addSlideCallback(id, std::forward<Callback>(callback), priority);
			return *this;
		}

		template <needs_same(geode::SliderNode), typename Callback>
		Build<T> addClickCallback(geode::ZStringView id, Callback&& callback, int priority = 0) {
			m_item->addClickCallback(id, std::forward<Callback>(callback), priority);
			return *this;
		}

		template <needs_same(geode::SliderNode), typename Callback>
		Build<T> addReleaseCallback(geode::ZStringView id, Callback&& callback, int priority = 0) {
			m_item->addReleaseCallback(id, std::forward<Callback>(callback), priority);
			return *this;
		}
		#endif

		// CCRGBAProtocol
		setter(CCRGBAProtocol, opacity, setOpacity, unsigned char)
		setter(CCRGBAProtocol, color, setColor, const ccColor3B&)
		setter(CCRGBAProtocol, cascadeOpacity, setCascadeOpacityEnabled, bool)
		setter(CCRGBAProtocol, cascadeColor, setCascadeColorEnabled, bool)

		template <needs_base(CCRGBAProtocol)>
		Build<T> color(float r, float g, float b) {
			return color(ccc3(r, g, b));
		}

		// CCLayer
		setter(CCLayer, registerTouchDispatcher, registerWithTouchDispatcher)
		setter(CCLayer, touchEnabled, setTouchEnabled, bool)
		setter(CCLayer, keyboardEnabled, setKeyboardEnabled, bool)
		setter(CCLayer, mouseEnabled, setMouseEnabled, bool)
		setter(CCLayer, touchMode, setTouchMode, ccTouchesMode)
		setter(CCLayer, touchPrio, setTouchPriority, bool)
		setter(CCLayer, keypad_enabled, setKeypadEnabled, bool)

		template <needs_base(CCLayer)>
		Build<T> initTouch() {
			touchEnabled(true).mouseEnabled(true).touchMode(kCCTouchesOneByOne);
			CCDirector::sharedDirector()->getTouchDispatcher()->registerForcePrio(m_item, 2);
			return *this;
		}

		template <needs_base(CCLayer)>
		Build<CCScene> intoScene() {
			auto scene = CCScene::create();
			scene->addChild(m_item);
			return Build<CCScene>(scene);
		}

		// CCScene
		template <needs_base(CCScene)>
		Build<T> pushScene() {
			CCDirector::sharedDirector()->pushScene(m_item);
			return *this;
		}

		template <needs_base(CCScene)>
		Build<T> replaceScene() {
			CCDirector::sharedDirector()->replaceScene(m_item);
			return *this;
		}

		template <needs_base(CCScene)>
		Build<CCTransitionFade> fadeIn(float fade) {
			return Build<CCTransitionFade>::create(fade, m_item);
		}


		// CCMenu
		setter(CCMenu, enabled, setEnabled, bool)

		// CCMenuItem
		setter(CCMenuItem, enabled, setEnabled, bool)
		setter(CCMenuItem, target, setTarget, CCObject*, SEL_MenuHandler)

		// geode::Toggler
		template <needs_same(geode::Toggler)>
		static Build<T> createToggle(char const* off, char const* on, geode::Toggler::ToggleCallback fn) {
			return Build<T>(geode::Toggler::createWithSpriteFrameNames(off, on, std::move(fn)));
		}

		template <needs_same(geode::Toggler)>
		static Build<T> createToggle(geode::Toggler::ToggleCallback fn) {
			return Build<T>(geode::Toggler::createWithStandardSprites(std::move(fn)));
		}

		template <needs_same(geode::Toggler)>
		static Build<T> createToggle(float scale, geode::Toggler::ToggleCallback fn) {
			return Build<T>(geode::Toggler::createWithStandardSprites(scale, std::move(fn)));
		}

		template <needs_same(geode::Toggler)>
		static Build<T> createToggle(CCNode* off, CCNode* on, geode::Toggler::ToggleCallback fn) {
			return Build<T>(geode::Toggler::create(off, on, std::move(fn)));
		}

		template <needs_same(geode::Toggler)>
		static Build<T> createToggle(char const* off, char const* on, float scale, geode::Toggler::ToggleCallback fn) {
			return Build<T>(geode::Toggler::createWithSpriteFrameNames(off, on, scale, std::move(fn)));
		}

		template <needs_same(geode::Toggler)>
		static Build<T> createToggleFiles(geode::ZStringView off, geode::ZStringView on, geode::Toggler::ToggleCallback fn) {
			return Build<T>(geode::Toggler::createWithSprites(off, on, std::move(fn)));
		}

		template <needs_same(geode::Toggler)>
		static Build<T> createToggleFiles(geode::ZStringView off, geode::ZStringView on, float scale, geode::Toggler::ToggleCallback fn) {
			return Build<T>(geode::Toggler::createWithSprites(off, on, scale, std::move(fn)));
		}

		template <needs_same(geode::Toggler)>
		Build<T> toggle(bool triggerCallback = false) {
			m_item->toggle(triggerCallback);
			return *this;
		}

		template <needs_same(geode::Toggler), typename Callback>
		Build<T> addToggleCallback(geode::ZStringView id, Callback&& callback, int priority = 0) {
			m_item->addToggleCallback(id, std::forward<Callback>(callback), priority);
			return *this;
		}

		template <needs_same(geode::Toggler)>
		Build<T> toggled(bool toggled, bool triggerCallback = false) {
			m_item->setToggled(toggled, triggerCallback);
			return *this;
		}

		// CCSprite
		template <needs_same(CCSprite)>
		static Build<T> createSpriteName(char const* frame) {
			return Build(CCSprite::createWithSpriteFrameName(frame));
		}

		setter(CCSprite, texture, setTexture, CCTexture2D*)
		setter(CCSprite, childColor, setChildColor, CCSprite*)
		setter(CCSprite, childOpacity, setChildOpacity, unsigned char)
		setter(CCSprite, textureRect, setTextureRect, const CCRect&)
		setter(CCSprite, displayFrame, setDisplayFrame, CCSpriteFrame*)
		setter(CCSprite, dirty, setDirty, bool)
		setter(CCSprite, flipX, setFlipX, bool)
		setter(CCSprite, flipY, setFlipY, bool)
		setter(CCSprite, blendFunc, setBlendFunc, ccBlendFunc)

		template <needs_base(CCNode)>
		Build<geode::Button> intoButton(geode::Button::ButtonCallback fn) {
			auto parent = m_item->getParent();
			if (parent)
				parent->removeChild(m_item);

			auto ret = Build<geode::Button>(geode::Button::createWithNode(m_item, std::move(fn)));
			if (parent) ret.parent(parent);
			return ret;
		}

		// same as intoButton except the callback can be with no args
		template <needs_base(CCNode)>
		Build<geode::Button> intoButton(FunctionType<void()> fn) {
			return intoButton([fn = std::move(fn)] (auto) mutable { fn(); });
		}

		template <needs_base(CCLabelProtocol)>
		Build<T> string(std::string const& str) {
			m_item->setString(str.c_str());
			return *this;
		}

		// CCLabelProtocol
		setter(CCLabelProtocol, string, setString, const char*)

		// geode::Label
		setter(geode::Label, text, setText, std::string)
		setter(geode::Label, alignment, setAlignment, geode::Label::Alignment)
		setter(geode::Label, maxWidth, setMaxWidth, float)
		setter(geode::Label, breakWords, setBreakWords, bool)
		setter(geode::Label, limitLabelWidth, setLimitLabelWidth, float, float, float)
		setter(geode::Label, kerning, setExtraKerning, float)
		setter(geode::Label, lineSpacing, setLineSpacing, float)
		setter(geode::Label, richText, setRichTextEnabled, bool)
		setter(geode::Label, font, setFont, geode::ZStringView, size_t)
		setter(geode::Label, richTextString, setRichText, std::string)
		setter(geode::Label, runs, setRuns, std::vector<geode::Label::Run>)
		setter(geode::Label, limitLabelHeight, setLimitLabelHeight, float, float, float)
		setter(geode::Label, limitLabelSize, setLimitLabelSize, CCSize, float, float)
		setter(geode::Label, removeLabelSizeLimit, removeLabelSizeLimit)
		setter(geode::Label, fitBox, setFitBox, CCSize, float)
		setter(geode::Label, disableFitBox, disableFitBox)
		setter(geode::Label, charColor, setCharColor, uint32_t, ccColor4B)
		setter(geode::Label, emojiRegistry, setEmojiRegistry, geode::EmojiRegistry const&)
		setter(geode::Label, disableEmojis, disableEmojis)
		setter(geode::Label, useColoredEmojis, setUseColoredEmojis, bool)
		setter(geode::Label, useColoredNodes, setUseColoredNodes, bool)

		// geode::ScrollLayer
		setter(geode::ScrollLayer, scrollWheel, enableScrollWheel, bool)
		setter(geode::ScrollLayer, scrollToFromTop, scrollToFromTop, float)
		setter(geode::ScrollLayer, scrollToTop, scrollToTop)
		setter(geode::ScrollLayer, cancelTouchLimit, setCancelTouchLimit, float)
		setter(geode::ScrollLayer, stealingTouches, setStealingTouches, bool)
		setter(geode::ScrollLayer, touchClipping, enableTouchClipping, bool)
		setter(geode::ScrollLayer, scrollClipping, enableScrollClipping, bool)
		setter(geode::ScrollLayer, stencil, setStencil, CCNode*)

		template <needs_same(geode::ScrollLayer)>
		Build<T> defaultListLayout(float gap = 2.5f) {
			m_item->m_contentLayer->setLayout(geode::ScrollLayer::createDefaultListLayout(gap));
			return *this;
		}

		// geode::Scrollbar
		setter(geode::Scrollbar, target, setTarget, CCScrollLayerExt*)
		setter(geode::Scrollbar, track, setTrack, geode::NineSlice*)
		setter(geode::Scrollbar, thumb, setThumb, geode::NineSlice*)
		setter(geode::Scrollbar, shrinkWithOverscroll, setShrinkWithOverscroll, bool)
		setter(geode::Scrollbar, touchInThumb, setTouchInThumb, bool)
		setter(geode::Scrollbar, style, setStyle, geode::Scrollbar::Style)
		setter(geode::Scrollbar, thumbColors, setThumbColors, ccColor4B, ccColor4B)

		// geode::BoxedLabel
		setter(geode::BoxedLabel, text, setText, std::string)
		setter(geode::BoxedLabel, font, setFont, geode::ZStringView)
		setter(geode::BoxedLabel, textScale, setTextScale, float)
		setter(geode::BoxedLabel, textAlignment, setTextAlignment, geode::Label::Alignment)
		setter(geode::BoxedLabel, maxWidth, setMaxWidth, float)
		setter(geode::BoxedLabel, padding, setPadding, float)
		setter(geode::BoxedLabel, boxSprite, setBoxSprite, geode::ZStringView)
		setter(geode::BoxedLabel, boxColor, setBoxColor, ccColor3B const&)
		setter(geode::BoxedLabel, boxOpacity, setBoxOpacity, GLubyte)
		setter(geode::BoxedLabel, boxScale, setBoxScale, float)
		setter(geode::BoxedLabel, lockWidth, lockWidth, bool)

		// ButtonSprite
		setter(ButtonSprite, string, setString, char const*)

		// SimplePlayer
		setter(SimplePlayer, secondColor, setSecondColor, ccColor3B const&)
		setter(SimplePlayer, playerFrame, updatePlayerFrame, int, IconType)
		setter(SimplePlayer, glowOutline, setGlowOutline, bool)

		/// Actions

		// CCAction
		setter(CCAction, target, setTarget, CCNode*)
		setter(CCAction, speedMod, setSpeedMod, float)

		template <needs_base(CCAction), typename U>
		Build<T> runFor(U node) {
			remove_build<U>()(node)->runAction(m_item);
			return *this;
		}

		// CCSequence

		template <needs_base(CCSequence), typename ...Args>
		static Build<T> create(Args... args) {
			return Build(CCSequence::create(args..., nullptr));
		}

		// CCFollow
		setter(CCFollow, boundarySet, setBoundarySet, bool)

		// CCFiniteTimeAction
		setter(CCFiniteTimeAction, duration, setDuration, float)

		template <needs_base(CCFiniteTimeAction)>
		Build<T> reverse() {
			m_item->reverse();
			return *this;
		}

		// CCCallFunc
		setter(CCCallFunc, target, setTargetCallback, CCObject*)

		// CCCallFuncO
		setter(CCCallFuncO, object, setObject, CCObject*)

		// CCActionInterval

		template <needs_base(CCActionInterval)>
		Build<CCRepeat> repeat(int times) {
			return Build<CCRepeat>::create(m_item, times);
		}

		template <needs_base(CCActionInterval)>
		Build<CCRepeatForever> repeatForever() {
			return Build<CCRepeatForever>::create(m_item);
		}

		template <needs_base(CCActionInterval), typename U> requires std::derived_from<remove_build_t<U>, CCFiniteTimeAction>
		Build<CCSequence> sequence(U action) {
			return Build<CCSequence>(CCSequence::create(m_item, remove_build<U>()(action), nullptr));
		}

		template <needs_base(CCActionInterval)>
		Build<CCSequence> sequence(FunctionType<void()> cb) {
			return sequence(BuildAction::create([fn = std::move(cb)](float dt) mutable { fn(); }));
		}

		template <needs_base(CCActionInterval)>
		Build<CCSequence> waitAfter(float d) {
			return sequence(Build<CCDelayTime>::create(d));
		}

		template <needs_base(CCActionInterval)>
		Build<CCSequence> waitBefore(float d) {
			return Build<CCDelayTime>::create(d).sequence(m_item);
		}

		// CCEaseRateAction
		setter(CCEaseRateAction, easeRate, setRate, float)

	};


	template <typename T>
	struct remove_build<Build<T>> {
		T* operator()(Build<T> a) { return a.collect(); }
	};

}

using uibuilder::Build;

#define $build_wrap(func) with([&](auto self) { self->func; })

#undef setter
#undef needs_base
#undef needs_same
#undef arg_1
#undef arg_2
#undef arg_3
#undef setter_0
#undef setter_1
#undef setter_2
#undef setter_3
#undef setter_n
#undef NLIST
#undef _CONCAT
#undef CONCAT

