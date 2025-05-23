Pod::Spec.new do |s|

  s.name         = "examples"
  s.version      = "1.0.0"
  s.summary      = "ccap ios examples"
  s.description  = <<-DESC
    ccap ios examples
  DESC

  s.homepage     = "https://github.com/wysaid/CameraCapture/tree/main/examples"

  s.author             = { "wysaid" => "this@wysaid.org" }

  s.source       = { :git => "git@github.com:wysaid/CameraCapture.git", :tag => "#{s.version}" }
  s.platform     = :ios, "13.0"

  s.source_files = [
      '*-*/**/*{h,hpp,c,cpp,mm,m}',
      'common/**/*{h,hpp,c,cpp,mm,m}',
      'window/**/*{h,hpp,c,cpp,mm,m}',
      'imgui/**/*{h,hpp,c,cpp,mm,m}',
    ]

  s.pod_target_xcconfig = {
    'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) CGE_COMPILE_WITH_METAL=1 USE_METAL=1',
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++17',
    'OTHER_CPLUSPLUSFLAGS' => '-fno-aligned-allocation'
  }

  # 增加当前 podspec 的目录 到 header search path
  s.xcconfig = { 'HEADER_SEARCH_PATHS' => '"${PROJECT_DIR}/.."' }

  s.requires_arc = true

  s.dependency 'ccap'
  s.framework = 'MetalKit'
end
