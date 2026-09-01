{
  "targets": [
    {
      "target_name": "c4dportal_virtual_camera",
      "sources": [
        "src/addon.cpp",
        "src/virtual_camera.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"],
      "conditions": [
        ["OS=='win'", {
          "libraries": ["mfplat.lib", "mf.lib", "mfreadwrite.lib", "mfuuid.lib", "mfsensorgroup.lib", "ole32.lib"],
          "msvs_settings": {
            "VCCLCompilerTool": { "ExceptionHandling": 1 }
          }
        }]
      ]
    }
  ]
}
