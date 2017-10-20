//
// Z3D - A structured light 3D scanner
// Copyright (C) 2013-2016 Nicolas Ulrich <nikolaseu@gmail.com>
//
// This file is part of Z3D.
//
// Z3D is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Z3D is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Z3D.  If not, see <http://www.gnu.org/licenses/>.
//

#include "zpyloncamera.h"

#include <QDebug>

#include <GenApi/INodeMap.h>

namespace Z3D
{

PylonCamera::BaslerImageHandler::BaslerImageHandler(PylonCamera *camera)
    : Pylon::CImageEventHandler()
    , m_camera(camera)
{
}

void PylonCamera::BaslerImageHandler::OnImageGrabbed(Pylon::CInstantCamera& camera, const Pylon::CGrabResultPtr& ptrGrabResult)
{
    const uint8_t* pImageBuffer = (uint8_t*)ptrGrabResult->GetBuffer();

    size_t width = ptrGrabResult->GetWidth();
    size_t height = ptrGrabResult->GetHeight();
    cv::Mat image(cv::Size(width, height), CV_8UC1,
        (unsigned char*)pImageBuffer, cv::Mat::AUTO_STEP);

    const int bytesPerPixel = 1;

    /// get image from buffer
    ZImageGrayscale::Ptr currentImage = m_camera->getNextBufferImage(width, height, 0, 0, bytesPerPixel);

    /// copy data
    currentImage->setBuffer((void *)pImageBuffer);

    /// set image number
    currentImage->setNumber(ptrGrabResult->GetFrameNumber());

    /// notify
    emit m_camera->newImageReceived(currentImage);
}

PylonCamera::PylonCamera(Pylon::IPylonDevice *device, QObject *parent)
    : ZCameraBase(parent)
{
    m_camera.Attach(device);

    m_image_handler = new BaslerImageHandler(this);

    m_camera.RegisterImageEventHandler(m_image_handler, Pylon::RegistrationMode_Append, Pylon::Cleanup_Delete);
    m_camera.Open();
}

PylonCamera::~PylonCamera()
{
    m_camera.Close();
    m_camera.DetachDevice();
}

bool PylonCamera::startAcquisition()
{
    if (!ZCameraBase::startAcquisition())
        return false;

    qDebug() << Q_FUNC_INFO;

    m_camera.StartGrabbing(Pylon::GrabStrategy_OneByOne, Pylon::GrabLoop_ProvidedByInstantCamera);

    return true;
}

bool PylonCamera::stopAcquisition()
{
    if (!ZCameraBase::stopAcquisition())
        return false;

    qDebug() << Q_FUNC_INFO;

    m_camera.StopGrabbing();

    return true;
}

QList<ZCameraInterface::ZCameraAttribute> PylonCamera::getAllAttributes()
{
    QList<ZCameraInterface::ZCameraAttribute> parameters;

    GenApi::INodeMap &nodemap = m_camera.GetNodeMap();
    GenApi::NodeList_t nodelist;
    nodemap.GetNodes(nodelist);

    for (const auto node : nodelist) {
        if (node->GetPrincipalInterfaceType() != GenApi::EInterfaceType::intfICategory) {
            continue;
        }

        QString categoryName = node->GetName();
        GenApi::NodeList_t childrens;
        node->GetChildren(childrens);

        for (const auto node : childrens) {
            ZCameraInterface::ZCameraAttribute attr;
            attr.readable = GenApi::IsReadable(node);
            attr.writable = GenApi::IsWritable(node);
            attr.id = node->GetName();
            attr.path = QString("%1::%2").arg(categoryName).arg(QString(node->GetName()));
            attr.description = node->GetDescription();
            attr.label = node->GetDisplayName();

            GenICam::gcstring_vector propertyNames;
            node->GetPropertyNames(propertyNames);
            QVariantMap properties;
            GenICam::gcstring values, attributes;
            for (const auto &property : propertyNames) {
                if (!node->GetProperty(property, values, attributes)) {
                    qWarning() << "unable to get property for" << attr.path;
                    break;
                }
                QVariantMap map;
                map["values"] = values.c_str();
                map["attributes"] = attributes.c_str();
                properties[QString(property.c_str())] = map;
            }
            qDebug() << attr.path << properties;

            switch (node->GetPrincipalInterfaceType()) {
            case GenApi::EInterfaceType::intfIValue:       //!< IValue interface
                qWarning() << "unhandled type GenApi::EInterfaceType::intfIValue" << attr.path;
                continue;
            case GenApi::EInterfaceType::intfIBase:        //!< IBase interface
                qWarning() << "unhandled type GenApi::EInterfaceType::intfIBase" << attr.path;
                continue;
            case GenApi::EInterfaceType::intfIInteger:     //!< IInteger interface
                attr.type =  ZCameraInterface::CameraAttributeTypeInt;
                attr.value = QString(values.c_str()).toInt();
                break;
            case GenApi::EInterfaceType::intfIBoolean:     //!< IBoolean interface
                attr.type =  ZCameraInterface::CameraAttributeTypeBool;
                attr.value = QString(values.c_str()).toInt() != 0;
                break;
            case GenApi::EInterfaceType::intfICommand:     //!< ICommand interface
                attr.type =  ZCameraInterface::CameraAttributeTypeCommand;
                break;
            case GenApi::EInterfaceType::intfIFloat:       //!< IFloat interface
                attr.type =  ZCameraInterface::CameraAttributeTypeFloat;
                attr.value = QString(values.c_str()).toDouble();
                break;
            case GenApi::EInterfaceType::intfIString:      //!< IString interface
                attr.type =  ZCameraInterface::CameraAttributeTypeString;
                attr.value = QString(values.c_str());
                break;
            case GenApi::EInterfaceType::intfIRegister:    //!< IRegister interface
                qWarning() << "unhandled type GenApi::EInterfaceType::intfIRegister" << attr.path;
                continue;
            case GenApi::EInterfaceType::intfICategory:    //!< ICategory interface
                qWarning() << "unhandled type GenApi::EInterfaceType::intfICategory" << attr.path;
                continue;
            case GenApi::EInterfaceType::intfIEnumeration: //!< IEnumeration interface
                attr.type =  ZCameraInterface::CameraAttributeTypeEnum;
                break;
            case GenApi::EInterfaceType::intfIEnumEntry:   //!< IEnumEntry interface
                qWarning() << "unhandled type GenApi::EInterfaceType::intfIEnumEntry" << attr.path;
                break;
            case GenApi::EInterfaceType::intfIPort:         //!< IPort interface
                qWarning() << "unhandled type GenApi::EInterfaceType::intfIPort" << attr.path;
                continue;
            default:
                qWarning() << "unknown type for" << attr.path;
            }

            parameters << attr;
        }
    }

    return parameters;
}

QVariant PylonCamera::getAttribute(const QString &name) const
{
    //! TODO
    return QVariant("INVALID");
}

bool PylonCamera::setAttribute(const QString &name, const QVariant &value, bool notify)
{
    //! TODO
    return false;
}

} // namespace Z3D