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
#include <string>

#include <ignition/common/Console.hh>
#include <ignition/msgs.hh>
#include <ignition/plugin/Register.hh>
#include <ignition/transport.hh>

namespace ignition::gazebo
{
  class CopyPastePrivate
  {
    public: std::string copiedData = "";

    public: transport::Node node;

    public: const std::string copyService = "/gui/copy";

    public: bool CopyServiceCB(const ignition::msgs::StringMsg &_req,
                ignition::msgs::Boolean &_resp);

    // things to hold here:
    //  - copiedData (std::string?)
    //    - I may need a way to know if anything is copied or not
    //      (maybe just checking if the std::string is empty is enough)
    //  - ign-transport stuff that can respond to service requests
    //    (the service cb could call either the copy or paste gui cb)
  };
}

using namespace ignition;
using namespace gazebo;

/////////////////////////////////////////////////
CopyPaste::CopyPaste()
  : ignition::gui::Plugin(),
  dataPtr(std::make_unique<CopyPastePrivate>())
{
  if (!this->dataPtr->node.Advertise(this->dataPtr->copyService,
        &CopyPastePrivate::CopyServiceCB, this->dataPtr.get()))
  {
    ignerr << "Error advertising service [" << this->dataPtr->copyService
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
bool CopyPastePrivate::CopyServiceCB(const ignition::msgs::StringMsg &_req,
    ignition::msgs::Boolean &_resp)
{
  this->copiedData = _req.data();
  ignerr << "copiedData is now [" << this->copiedData << "]" << std::endl;
  _resp.set_data(true);
  return true;
}

// Register this plugin
IGNITION_ADD_PLUGIN(ignition::gazebo::CopyPaste,
                    ignition::gui::Plugin)
