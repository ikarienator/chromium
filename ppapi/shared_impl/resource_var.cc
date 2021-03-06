// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/resource_var.h"

#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {

ResourceVar::ResourceVar() : pp_resource_(0) {}

ResourceVar::ResourceVar(PP_Resource pp_resource) : pp_resource_(pp_resource) {}

ResourceVar::ResourceVar(const IPC::Message& creation_message)
    : pp_resource_(0),
      creation_message_(creation_message) {}

ResourceVar::~ResourceVar() {}

bool ResourceVar::IsPending() const {
  return pp_resource_ == 0 && creation_message_.type() != 0;
}

ResourceVar* ResourceVar::AsResourceVar() {
  return this;
}

PP_VarType ResourceVar::GetType() const {
  return PP_VARTYPE_RESOURCE;
}

// static
ResourceVar* ResourceVar::FromPPVar(PP_Var var) {
  if (var.type != PP_VARTYPE_RESOURCE)
    return NULL;
  scoped_refptr<Var> var_object(
      PpapiGlobals::Get()->GetVarTracker()->GetVar(var));
  if (!var_object.get())
    return NULL;
  return var_object->AsResourceVar();
}

}  // namespace ppapi
