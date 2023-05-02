﻿// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using Microsoft.UI.Xaml.Media;

namespace ArchivePreviewHandler.Helpers
{
    public interface IIconManager
    {
        ImageSource GetFileIcon(string fileName);

        ImageSource GetDirectoryIcon();
    }
}