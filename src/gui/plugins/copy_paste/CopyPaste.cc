/*
 * Copyright (C) 2021 Open Source Robotics Foundation
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

#include "CopyPaste.hh"

#include <memory>
#include <mutex>
#include <string>

#include <ignition/common/Console.hh>
#include <ignition/msgs.hh>
#include <ignition/plugin/Register.hh>
#include <ignition/transport.hh>

#include "ignition/gazebo/EntityComponentManager.hh"
#include "ignition/gazebo/components/Light.hh"
//#include "ignition/gazebo/components/Model.hh"
#include "ignition/gazebo/components/Name.hh"

namespace ignition::gazebo
{
  class CopyPastePrivate
  {
    public: std::string copiedData = "";

    public: transport::Node node;

    public: const std::string copyService = "/gui/copy";

    public: const std::string pasteService = "/gui/paste";

    /// \brief Paste a light entity
    /// \param[in] _entity The light entity to copy
    /// \param[in] _ecm The entity component manager
    /// \return True if _entity is a light entity and was pasted.
    /// False otherwise
    public: bool PasteLight(const Entity _entity,
                const EntityComponentManager &_ecm);

    public: void Paste();

    public: bool CopyServiceCB(const ignition::msgs::StringMsg &_req,
                ignition::msgs::Boolean &_resp);

    public: bool PasteServiceCB(const ignition::msgs::Empty &_req,
                ignition::msgs::Boolean &_resp);

    /// \brief A flag that indicates whether data should be pasted or not
    public: bool paste = false;

    /// \brief A mutex to ensure that there are no race conditions between
    /// copy/paste
    public: std::mutex mutex;
  };
}

using namespace ignition;
using namespace gazebo;

/////////////////////////////////////////////////
CopyPaste::CopyPaste()
  : GuiSystem(), dataPtr(std::make_unique<CopyPastePrivate>())
{
  if (!this->dataPtr->node.Advertise(this->dataPtr->copyService,
        &CopyPastePrivate::CopyServiceCB, this->dataPtr.get()))
  {
    ignerr << "Error advertising service [" << this->dataPtr->copyService
      << "]" << std::endl;
  }

  if (!this->dataPtr->node.Advertise(this->dataPtr->pasteService,
        &CopyPastePrivate::PasteServiceCB, this->dataPtr.get()))
  {
    ignerr << "Error advertising service [" << this->dataPtr->pasteService
      << "]" << std::endl;
  }
}

/////////////////////////////////////////////////
CopyPaste::~CopyPaste() = default;

/////////////////////////////////////////////////
void CopyPaste::LoadConfig(const tinyxml2::XMLElement *)
{
  if (this->title.empty())
    this->title = "Copy/Paste";
}

/////////////////////////////////////////////////
void CopyPaste::Update(const UpdateInfo &/*_info*/,
    EntityComponentManager &_ecm)
{
  std::lock_guard<std::mutex> guard(this->dataPtr->mutex);

  if (this->dataPtr->paste)
  {
    ignerr << "about to paste..." << std::endl;
    auto entities =
      _ecm.EntitiesByComponents(components::Name(this->dataPtr->copiedData));
    if (entities.empty())
    {
      ignerr << "Requested to paste an entity named ["
        << this->dataPtr->copiedData
        << "], but the ecm has no entities with this name. "
        << "The paste request will be ignored." << std::endl;
    }
    else if (entities.size() > 1)
    {
      ignerr << "Requested to paste an entity named ["
        << this->dataPtr->copiedData
        << "], but the ecm has more than one entity with this name. "
        << "The paste request will be ignored." << std::endl;
    }
    else
    {
      auto entity = entities[0];
      ignerr << "pasting entity [" << entity << "]" << std::endl;
      /*
       *auto modelSdf = _ecm.Component<components::ModelSdf>(entity);
       *if (nullptr == modelSdf)
       *{
       *  ignerr << "Requested to paste entity [" << entity
       *    << "], but this entity has no ModelSdf component. "
       *    << "The paste request will be ignored." << std::endl;
       *}
       *else
       *{
       *  auto sdfElement = modelSdf->Data().Element();
       *  auto sdfString = sdfElement->ToString("");
       *  ignerr << sdfString << std::endl;
       *}
       */
      this->dataPtr->PasteLight(entity, _ecm);
      // get the entity's SDF element (may need to make a copy)
      // make the name unique (add a "_copyN" suffix to the name?)
      // get string representation of SDF with unique name
      // use SpawnFromDescription
    }
  }

  this->dataPtr->paste = false;
}

/////////////////////////////////////////////////
void CopyPaste::OnCopy(const QString &_copyItemName)
{
  std::lock_guard<std::mutex> guard(this->dataPtr->mutex);
  this->dataPtr->copiedData = _copyItemName.toStdString();
}

/////////////////////////////////////////////////
void CopyPaste::OnPaste()
{
  this->dataPtr->Paste();
}

/////////////////////////////////////////////////
bool CopyPastePrivate::PasteLight(const Entity _entity,
    const EntityComponentManager &_ecm)
{
  auto lightCompPtr = _ecm.Component<components::Light>(_entity);
  if (nullptr == lightCompPtr)
    return false;

  auto sdfElementPtr = lightCompPtr->Data().Element();
  ignerr << lightCompPtr->Data().Name() << std::endl;
  if (nullptr == sdfElementPtr)
  {
    ignerr << "element is null!\n";
    return false;
  }
  auto sdfString = sdfElementPtr->ToString("");
  ignerr << sdfString << std::endl;
  // TODO call SpawnFromDescription once I have a string representation of
  // the SDF for the light - I may need to build the string manually since
  // the sdf::light that was serialized to the GUI does not have the elementPtr
  return true;
}

/////////////////////////////////////////////////
void CopyPastePrivate::Paste()
{
  std::lock_guard<std::mutex> guard(this->mutex);

  // we should only paste if something has been copied
  if (!this->copiedData.empty())
    this->paste = true;
}

/////////////////////////////////////////////////
bool CopyPastePrivate::CopyServiceCB(const ignition::msgs::StringMsg &_req,
    ignition::msgs::Boolean &_resp)
{
  {
    std::lock_guard<std::mutex> guard(this->mutex);
    this->copiedData = _req.data();
  }
  ignerr << "copiedData is now [" << this->copiedData << "]" << std::endl;
  _resp.set_data(true);
  return true;
}

/////////////////////////////////////////////////
bool CopyPastePrivate::PasteServiceCB(const ignition::msgs::Empty &/*_req*/,
    ignition::msgs::Boolean &_resp)
{
  this->Paste();
  _resp.set_data(true);
  return true;
}

// Register this plugin
IGNITION_ADD_PLUGIN(ignition::gazebo::CopyPaste,
                    ignition::gui::Plugin)
