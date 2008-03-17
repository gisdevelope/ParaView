/*=========================================================================

  Program:   ParaView
  Module:    vtkPVSelectionSource.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPVSelectionSource.h"

#include "vtkObjectFactory.h"
#include "vtkSelection.h"
#include "vtkSelectionSource.h"
#include "vtkInformationVector.h"
#include "vtkInformation.h"
#include "vtkStreamingDemandDrivenPipeline.h"

#include <vtkstd/vector>
#include <vtkstd/set>

class vtkPVSelectionSource::vtkInternal
{
public:
  struct IDType
    {
    vtkIdType Piece;
    vtkIdType ID;
    IDType(vtkIdType piece, vtkIdType id)
      {
      this->Piece = piece;
      this->ID = id;
      }

    bool operator < (const IDType& other) const
      {
      if (this->Piece == other.Piece)
        {
        return (this->ID < other.ID);
        }
      return (this->Piece < other.Piece);
      }
    };

  struct CompositeIDType
    {
    unsigned int CompositeIndex;
    vtkIdType Piece;
    vtkIdType ID;
    CompositeIDType(unsigned int ci, vtkIdType piece, vtkIdType id)
      {
      this->CompositeIndex = ci;
      this->Piece = piece;
      this->ID = id;
      }
    bool operator < (const CompositeIDType& other) const
      {
      if (this->CompositeIndex == other.CompositeIndex)
        {
        if (this->Piece == other.Piece)
          {
          return (this->ID < other.ID);
          }
        return (this->Piece < other.Piece);
        }
      return (this->CompositeIndex < other.CompositeIndex);
      }
    };

  struct HierarchicalIDType
    {
    unsigned int Level;
    unsigned int DataSet;
    vtkIdType ID;
    HierarchicalIDType(unsigned int level, unsigned int ds, vtkIdType id)
      {
      this->Level = level;
      this->DataSet = ds;
      this->ID = id;
      }
    bool operator < (const HierarchicalIDType& other) const
      {
      if (this->Level == other.Level)
        {
        if (this->DataSet == other.DataSet)
          {
          return (this->ID < other.ID);
          }
        return (this->DataSet < other.DataSet);
        }
      return (this->Level < other.Level);
      }
    };

  typedef vtkstd::set<vtkIdType> SetOfIDs;
  typedef vtkstd::set<IDType> SetOfIDType;
  typedef vtkstd::set<CompositeIDType> SetOfCompositeIDType;
  typedef vtkstd::set<HierarchicalIDType> SetOfHierarchicalIDType;
  typedef vtkstd::vector<double> VectorOfDoubles;


  SetOfIDs GlobalIDs;
  SetOfIDType IDs;
  SetOfCompositeIDType CompositeIDs;
  SetOfHierarchicalIDType HierarchicalIDs;
  VectorOfDoubles Locations;
  VectorOfDoubles Thresholds;
};


vtkStandardNewMacro(vtkPVSelectionSource);
vtkCxxRevisionMacro(vtkPVSelectionSource, "1.1");
//----------------------------------------------------------------------------
vtkPVSelectionSource::vtkPVSelectionSource()
{
  this->SetNumberOfInputPorts(0);
  this->SetNumberOfOutputPorts(1);

  this->Internal = new vtkInternal();
  this->Mode = FRUSTUM;
  this->ContainingCells = 1;
  this->Inverse = 0;
  this->ArrayName = NULL;
  for (int cc=0; cc < 32; cc++)
    {
    this->Frustum[cc] = 0;
    }
}

//----------------------------------------------------------------------------
vtkPVSelectionSource::~vtkPVSelectionSource()
{
  this->SetArrayName(0);
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkPVSelectionSource::SetFrustum(double vertices[32])
{
  memcpy(this->Frustum, vertices, sizeof(double)*32);
  this->Mode = FRUSTUM;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkPVSelectionSource::AddGlobalID(vtkIdType id)
{
  this->Mode = GLOBALIDS;
  this->Internal->GlobalIDs.insert(id);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkPVSelectionSource::RemoveAllGlobalIDs()
{
  this->Mode = GLOBALIDS;
  this->Internal->GlobalIDs.clear();
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkPVSelectionSource::AddID(vtkIdType piece, vtkIdType id)
{
  if (piece < -1)
    {
    piece = -1;
    }
  this->Mode = ID;
  this->Internal->IDs.insert(vtkInternal::IDType(piece, id));
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkPVSelectionSource::RemoveAllIDs()
{
  this->Mode = ID;
  this->Internal->IDs.clear();
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkPVSelectionSource::AddCompositeID(unsigned int composite_index,
  vtkIdType piece, vtkIdType id)
{
  if (piece < -1)
    {
    piece = -1;
    }

  this->Mode = COMPOSITEID;
  this->Internal->CompositeIDs.insert(
    vtkInternal::CompositeIDType(composite_index, piece, id));
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkPVSelectionSource::RemoveAllCompositeIDs()
{
  this->Mode = COMPOSITEID;
  this->Internal->CompositeIDs.clear();
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkPVSelectionSource::AddHierarhicalID(
  unsigned int level, unsigned int dataset, vtkIdType id)
{
  this->Mode = HIERARCHICALID;
  this->Internal->HierarchicalIDs.insert(
    vtkInternal::HierarchicalIDType(level, dataset, id));
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkPVSelectionSource::RemoveAllHierarchicalIDs()
{
  this->Mode = HIERARCHICALID;
  this->Internal->HierarchicalIDs.clear();
  this->Modified();
}


//----------------------------------------------------------------------------
void vtkPVSelectionSource::AddThreshold(double min, double max)
{
  this->Mode = THRESHOLDS;
  this->Internal->Thresholds.push_back(min);
  this->Internal->Thresholds.push_back(max);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkPVSelectionSource::RemoveAllThresholds()
{
  this->Mode = THRESHOLDS;
  this->Internal->Thresholds.clear();
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkPVSelectionSource::SetArrayName(const char* arrayName)
{
  this->Mode = THRESHOLDS;

  if (this->ArrayName == NULL && arrayName == NULL) 
    { 
    return;
    }

  if (this->ArrayName && arrayName && strcmp(this->ArrayName, arrayName) == 0)
    { 
    return;
    }

  delete [] this->ArrayName;
  this->ArrayName = 0;
  if (arrayName)
    {
    size_t n = strlen(arrayName) + 1;
    char *cp1 =  new char[n];
    const char *cp2 = (arrayName);
    this->ArrayName = cp1;
    do 
      {
      *cp1++ = *cp2++; 
      } 
    while (--n);
    }

  this->Modified();
}

//----------------------------------------------------------------------------
void vtkPVSelectionSource::AddLocation(double x, double y, double z)
{
  this->Mode = LOCATIONS;
  this->Internal->Locations.push_back(x);
  this->Internal->Locations.push_back(y);
  this->Internal->Locations.push_back(z);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkPVSelectionSource::RemoveAllLocations()
{
  this->Mode = LOCATIONS;
  this->Internal->Locations.clear();
  this->Modified();
}

//----------------------------------------------------------------------------
int vtkPVSelectionSource::RequestData(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** vtkNotUsed(inputVector),
  vtkInformationVector* outputVector)
{
  vtkSelection* output = vtkSelection::GetData(outputVector);
  output->Clear();

  int piece = 0;
  int npieces = -1;
  vtkInformation *outInfo = outputVector->GetInformationObject(0);
  if (outInfo->Has(
        vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER()))
    {
    piece = outInfo->Get(
      vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER());
    }
  if (outInfo->Has(
      vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES()))
    {

    npieces = outInfo->Get(
      vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES());
    }

  vtkSelectionSource* source = vtkSelectionSource::New();
  vtkStreamingDemandDrivenPipeline* sddp = vtkStreamingDemandDrivenPipeline::SafeDownCast(
    source->GetExecutive());
  if (sddp)
    {
    sddp->SetUpdateExtent(0, piece, npieces, 0);
    }

  source->SetFieldType(this->FieldType);
  source->SetContainingCells(this->ContainingCells);
  source->SetInverse(this->Inverse);

  switch (this->Mode)
    {
  case FRUSTUM:
    source->SetContentType(vtkSelection::FRUSTUM);
    source->SetFrustum(this->Frustum);
    source->Update();
    output->ShallowCopy(source->GetOutput());
    break;

  case GLOBALIDS:
      {
      source->SetContentType(vtkSelection::GLOBALIDS);
      source->RemoveAllIDs();
      vtkInternal::SetOfIDs::iterator iter;
      for (iter = this->Internal->GlobalIDs.begin();
        iter != this->Internal->GlobalIDs.end(); ++iter)
        {
        source->AddID(-1, *iter);
        }
      source->Update();
      output->ShallowCopy(source->GetOutput());
      }
    break;

  case COMPOSITEID:
      {
      output->SetContentType(vtkSelection::SELECTIONS);
      source->SetContentType(vtkSelection::INDICES);
      vtkInternal::SetOfCompositeIDType::iterator iter;
      for (iter = this->Internal->CompositeIDs.begin();
        iter != this->Internal->CompositeIDs.end(); ++iter)
        {
        if (iter == this->Internal->CompositeIDs.begin() ||
          source->GetCompositeIndex() != static_cast<int>(iter->CompositeIndex))
          {
          if (iter != this->Internal->CompositeIDs.begin())
            {
            source->Update();
            vtkSelection* clone = vtkSelection::New();
            clone->ShallowCopy(source->GetOutput());
            output->AddChild(clone);
            clone->Delete();
            }

          source->RemoveAllIDs();
          source->SetCompositeIndex(
            static_cast<int>(iter->CompositeIndex));
          }
        source->AddID(iter->Piece, iter->ID);
        }
      if (this->Internal->CompositeIDs.size() > 0)
        {
        source->Update();
        vtkSelection* clone = vtkSelection::New();
        clone->ShallowCopy(source->GetOutput());
        output->AddChild(clone);
        clone->Delete();
        }
      }
    break;

  case HIERARCHICALID:
      {
      output->SetContentType(vtkSelection::SELECTIONS);
      source->SetContentType(vtkSelection::INDICES);
      vtkInternal::SetOfHierarchicalIDType::iterator iter;
      for (iter = this->Internal->HierarchicalIDs.begin();
        iter != this->Internal->HierarchicalIDs.end(); ++iter)
        {
        if (source->GetHierarchicalLevel() != static_cast<int>(iter->Level) ||
          source->GetHierarchicalIndex() != static_cast<int>(iter->DataSet))
          {
          if (iter != this->Internal->HierarchicalIDs.begin())
            {
            source->Update();
            vtkSelection* clone = vtkSelection::New();
            clone->ShallowCopy(source->GetOutput());
            output->AddChild(clone);
            clone->Delete();
            }

          source->RemoveAllIDs();
          source->SetHierarchicalLevel(static_cast<int>(iter->Level));
          source->SetHierarchicalIndex(static_cast<int>(iter->DataSet));
          }
        source->AddID(-1, iter->ID);
        }
      if (this->Internal->HierarchicalIDs.size() > 0)
        {
        source->Update();
        vtkSelection* clone = vtkSelection::New();
        clone->ShallowCopy(source->GetOutput());
        output->AddChild(clone);
        clone->Delete();
        }
      }
    break;

  case THRESHOLDS:
      {
      source->SetContentType(vtkSelection::THRESHOLDS);
      vtkInternal::VectorOfDoubles::iterator iter;
      for (iter = this->Internal->Thresholds.begin();
        iter != this->Internal->Thresholds.end(); )
        {
        double min = *iter;
        iter++;
        double max = *iter;
        iter++;
        source->AddThreshold(min, max);
        }
      source->Update();
      output->ShallowCopy(source->GetOutput());
      }
    break;

  case LOCATIONS:
      {
      source->SetContentType(vtkSelection::LOCATIONS);
      vtkInternal::VectorOfDoubles::iterator iter;
      for (iter = this->Internal->Locations.begin();
        iter != this->Internal->Locations.end(); )
        {
        double x = *iter;
        iter++;
        double y = *iter;
        iter++;
        double z = *iter;
        iter++;
        source->AddLocation(x, y ,z);
        }
      source->Update();
      output->ShallowCopy(source->GetOutput());
      }
    break;

  case ID:
  default:
      {
      source->SetContentType(vtkSelection::INDICES);
      source->RemoveAllIDs();
      vtkInternal::SetOfIDType::iterator iter;
      for (iter = this->Internal->IDs.begin();
        iter != this->Internal->IDs.end(); ++iter)
        {
        source->AddID(iter->Piece, iter->ID);
        }
      source->Update();
      output->ShallowCopy(source->GetOutput());
      }
    break;
    }
   source->Delete();
   return 1;
}

//----------------------------------------------------------------------------
void vtkPVSelectionSource::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}


