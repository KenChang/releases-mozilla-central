/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLOptionElement.h"
#include "mozilla/dom/HTMLOptionElementBinding.h"
#include "nsHTMLSelectElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIDOMNode.h"
#include "nsIDOMHTMLCollection.h"
#include "nsISelectControlFrame.h"

// Notify/query select frame for selected state
#include "nsIFormControlFrame.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsNodeInfoManager.h"
#include "nsCOMPtr.h"
#include "nsEventStates.h"
#include "nsContentCreatorFunctions.h"
#include "mozAutoDocUpdate.h"

/**
 * Implementation of &lt;option&gt;
 */

nsGenericHTMLElement*
NS_NewHTMLOptionElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                        mozilla::dom::FromParser aFromParser)
{
  /*
   * HTMLOptionElement's will be created without a nsINodeInfo passed in
   * if someone says "var opt = new Option();" in JavaScript, in a case like
   * that we request the nsINodeInfo from the document's nodeinfo list.
   */
  nsCOMPtr<nsINodeInfo> nodeInfo(aNodeInfo);
  if (!nodeInfo) {
    nsCOMPtr<nsIDocument> doc =
      do_QueryInterface(nsContentUtils::GetDocumentFromCaller());
    NS_ENSURE_TRUE(doc, nullptr);

    nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::option, nullptr,
                                                   kNameSpaceID_XHTML,
                                                   nsIDOMNode::ELEMENT_NODE);
    NS_ENSURE_TRUE(nodeInfo, nullptr);
  }

  return new mozilla::dom::HTMLOptionElement(nodeInfo.forget());
}

DOMCI_NODE_DATA(HTMLOptionElement, mozilla::dom::HTMLOptionElement)

namespace mozilla {
namespace dom {

HTMLOptionElement::HTMLOptionElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mSelectedChanged(false),
    mIsSelected(false),
    mIsInSetDefaultSelected(false)
{
  SetIsDOMBinding();

  // We start off enabled
  AddStatesSilently(NS_EVENT_STATE_ENABLED);
}

HTMLOptionElement::~HTMLOptionElement()
{
}

// ISupports


NS_IMPL_ADDREF_INHERITED(HTMLOptionElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLOptionElement, Element)


// QueryInterface implementation for HTMLOptionElement
NS_INTERFACE_TABLE_HEAD(HTMLOptionElement)
  NS_HTML_CONTENT_INTERFACE_TABLE2(HTMLOptionElement,
                                   nsIDOMHTMLOptionElement,
                                   nsIJSNativeInitializer)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLOptionElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLOptionElement)


NS_IMPL_ELEMENT_CLONE(HTMLOptionElement)


NS_IMETHODIMP
HTMLOptionElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  NS_IF_ADDREF(*aForm = GetForm());
  return NS_OK;
}

nsHTMLFormElement*
HTMLOptionElement::GetForm()
{
  nsHTMLSelectElement* selectControl = GetSelect();
  return selectControl ? selectControl->GetForm() : nullptr;
}

void
HTMLOptionElement::SetSelectedInternal(bool aValue, bool aNotify)
{
  mSelectedChanged = true;
  mIsSelected = aValue;

  // When mIsInSetDefaultSelected is true, the state change will be handled by
  // SetAttr/UnsetAttr.
  if (!mIsInSetDefaultSelected) {
    UpdateState(aNotify);
  }
}

NS_IMETHODIMP
HTMLOptionElement::GetSelected(bool* aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = Selected();
  return NS_OK;
}

NS_IMETHODIMP
HTMLOptionElement::SetSelected(bool aValue)
{
  // Note: The select content obj maintains all the PresState
  // so defer to it to get the answer
  nsHTMLSelectElement* selectInt = GetSelect();
  if (selectInt) {
    int32_t index;
    GetIndex(&index);
    // This should end up calling SetSelectedInternal
    return selectInt->SetOptionsSelectedByIndex(index, index, aValue,
                                                false, true, true,
                                                nullptr);
  } else {
    SetSelectedInternal(aValue, true);
    return NS_OK;
  }

  return NS_OK;
}

NS_IMPL_BOOL_ATTR(HTMLOptionElement, DefaultSelected, selected)
// GetText returns a whitespace compressed .textContent value.
NS_IMPL_STRING_ATTR_WITH_FALLBACK(HTMLOptionElement, Label, label, GetText)
NS_IMPL_STRING_ATTR_WITH_FALLBACK(HTMLOptionElement, Value, value, GetText)
NS_IMPL_BOOL_ATTR(HTMLOptionElement, Disabled, disabled)

NS_IMETHODIMP
HTMLOptionElement::GetIndex(int32_t* aIndex)
{
  // When the element is not in a list of options, the index is 0.
  *aIndex = 0;

  // Only select elements can contain a list of options.
  nsHTMLSelectElement* selectElement = GetSelect();
  if (!selectElement) {
    return NS_OK;
  }

  nsHTMLOptionCollection* options = selectElement->GetOptions();
  if (!options) {
    return NS_OK;
  }

  // aIndex will not be set if GetOptionsIndex fails.
  return options->GetOptionIndex(this, 0, true, aIndex);
}

bool
HTMLOptionElement::Selected() const
{
  // If we haven't been explictly selected or deselected, use our default value
  if (!mSelectedChanged) {
    return DefaultSelected();
  }

  return mIsSelected;
}

bool
HTMLOptionElement::DefaultSelected() const
{
  return HasAttr(kNameSpaceID_None, nsGkAtoms::selected);
}

nsChangeHint
HTMLOptionElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                          int32_t aModType) const
{
  nsChangeHint retval =
      nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType);

  if (aAttribute == nsGkAtoms::label ||
      aAttribute == nsGkAtoms::text) {
    NS_UpdateHint(retval, NS_STYLE_HINT_REFLOW);
  }
  return retval;
}

nsresult
HTMLOptionElement::BeforeSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::BeforeSetAttr(aNamespaceID, aName,
                                                    aValue, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNamespaceID != kNameSpaceID_None || aName != nsGkAtoms::selected ||
      mSelectedChanged) {
    return NS_OK;
  }

  // We just changed out selected state (since we look at the "selected"
  // attribute when mSelectedChanged is false).  Let's tell our select about
  // it.
  nsHTMLSelectElement* selectInt = GetSelect();
  if (!selectInt) {
    return NS_OK;
  }

  // Note that at this point mSelectedChanged is false and as long as that's
  // true it doesn't matter what value mIsSelected has.
  NS_ASSERTION(!mSelectedChanged, "Shouldn't be here");

  bool newSelected = (aValue != nullptr);
  bool inSetDefaultSelected = mIsInSetDefaultSelected;
  mIsInSetDefaultSelected = true;

  int32_t index;
  GetIndex(&index);
  // This should end up calling SetSelectedInternal, which we will allow to
  // take effect so that parts of SetOptionsSelectedByIndex that might depend
  // on it working don't get confused.
  rv = selectInt->SetOptionsSelectedByIndex(index, index, newSelected,
                                            false, true, aNotify,
                                            nullptr);

  // Now reset our members; when we finish the attr set we'll end up with the
  // rigt selected state.
  mIsInSetDefaultSelected = inSetDefaultSelected;
  mSelectedChanged = false;
  // mIsSelected doesn't matter while mSelectedChanged is false

  return rv;
}

NS_IMETHODIMP
HTMLOptionElement::GetText(nsAString& aText)
{
  nsAutoString text;

  nsIContent* child = nsINode::GetFirstChild();
  while (child) {
    if (child->NodeType() == nsIDOMNode::TEXT_NODE ||
        child->NodeType() == nsIDOMNode::CDATA_SECTION_NODE) {
      child->AppendTextTo(text);
    }
    if (child->IsHTML(nsGkAtoms::script) || child->IsSVG(nsGkAtoms::script)) {
      child = child->GetNextNonChildNode(this);
    } else {
      child = child->GetNextNode(this);
    }
  }

  // XXX No CompressWhitespace for nsAString.  Sad.
  text.CompressWhitespace(true, true);
  aText = text;

  return NS_OK;
}

NS_IMETHODIMP
HTMLOptionElement::SetText(const nsAString& aText)
{
  return nsContentUtils::SetNodeTextContent(this, aText, true);
}

nsresult
HTMLOptionElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // Our new parent might change :disabled/:enabled state.
  UpdateState(false);

  return NS_OK;
}

void
HTMLOptionElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);

  // Our previous parent could have been involved in :disabled/:enabled state.
  UpdateState(false);
}

nsEventStates
HTMLOptionElement::IntrinsicState() const
{
  nsEventStates state = nsGenericHTMLElement::IntrinsicState();
  if (Selected()) {
    state |= NS_EVENT_STATE_CHECKED;
  }
  if (DefaultSelected()) {
    state |= NS_EVENT_STATE_DEFAULT;
  }

  // An <option> is disabled if it has @disabled set or if it's <optgroup> has
  // @disabled set.
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    state |= NS_EVENT_STATE_DISABLED;
    state &= ~NS_EVENT_STATE_ENABLED;
  } else {
    nsIContent* parent = GetParent();
    if (parent && parent->IsHTML(nsGkAtoms::optgroup) &&
        parent->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
      state |= NS_EVENT_STATE_DISABLED;
      state &= ~NS_EVENT_STATE_ENABLED;
    } else {
      state &= ~NS_EVENT_STATE_DISABLED;
      state |= NS_EVENT_STATE_ENABLED;
    }
  }

  return state;
}

// Get the select content element that contains this option
nsHTMLSelectElement*
HTMLOptionElement::GetSelect()
{
  nsIContent* parent = this;
  while ((parent = parent->GetParent()) &&
         parent->IsHTML()) {
    nsHTMLSelectElement* select = nsHTMLSelectElement::FromContent(parent);
    if (select) {
      return select;
    }
    if (parent->Tag() != nsGkAtoms::optgroup) {
      break;
    }
  }

  return nullptr;
}

NS_IMETHODIMP
HTMLOptionElement::Initialize(nsISupports* aOwner,
                              JSContext* aContext,
                              JSObject *aObj,
                              uint32_t argc,
                              jsval *argv)
{
  nsresult result = NS_OK;

  if (argc > 0) {
    // The first (optional) parameter is the text of the option
    JSString* jsstr = JS_ValueToString(aContext, argv[0]);
    if (!jsstr) {
      return NS_ERROR_FAILURE;
    }

    // Create a new text node and append it to the option
    nsCOMPtr<nsIContent> textContent;
    result = NS_NewTextNode(getter_AddRefs(textContent),
                            mNodeInfo->NodeInfoManager());
    if (NS_FAILED(result)) {
      return result;
    }

    size_t length;
    const jschar *chars = JS_GetStringCharsAndLength(aContext, jsstr, &length);
    if (!chars) {
      return NS_ERROR_FAILURE;
    }

    textContent->SetText(chars, length, false);

    result = AppendChildTo(textContent, false);
    if (NS_FAILED(result)) {
      return result;
    }

    if (argc > 1) {
      // The second (optional) parameter is the value of the option
      jsstr = JS_ValueToString(aContext, argv[1]);
      if (!jsstr) {
        return NS_ERROR_FAILURE;
      }

      size_t length;
      const jschar *chars = JS_GetStringCharsAndLength(aContext, jsstr, &length);
      if (!chars) {
        return NS_ERROR_FAILURE;
      }

      // Set the value attribute for this element
      nsAutoString value(chars, length);

      result = SetAttr(kNameSpaceID_None, nsGkAtoms::value, value,
                       false);
      if (NS_FAILED(result)) {
        return result;
      }

      if (argc > 2) {
        // The third (optional) parameter is the defaultSelected value
        JSBool defaultSelected;
        JS_ValueToBoolean(aContext, argv[2], &defaultSelected);
        if (defaultSelected) {
          result = SetAttr(kNameSpaceID_None, nsGkAtoms::selected,
                           EmptyString(), false);
          NS_ENSURE_SUCCESS(result, result);
        }

        // XXX This is *untested* behavior.  Should work though.
        if (argc > 3) {
          JSBool selected;
          JS_ValueToBoolean(aContext, argv[3], &selected);

          return SetSelected(selected);
        }
      }
    }
  }

  return result;
}

nsresult
HTMLOptionElement::CopyInnerTo(Element* aDest)
{
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDest->OwnerDoc()->IsStaticDocument()) {
    static_cast<HTMLOptionElement*>(aDest)->SetSelected(Selected());
  }
  return NS_OK;
}

JSObject*
HTMLOptionElement::WrapNode(JSContext* aCx, JSObject* aScope, bool* aTriedToWrap)
{
  return HTMLOptionElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

} // namespace dom
} // namespace mozilla
