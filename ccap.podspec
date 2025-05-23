Pod::Spec.new do |s|
  s.name         = "ccap"
  s.version      = "1.0.0"
  s.summary      = "CameraCapture And Player"
  s.description  = <<-DESC
Pod of https://github.com/wysaid/CameraCapture
DESC


  s.homepage     = "https://github.com/wysaid/CameraCapture"

  s.license      = "MIT"
  
  s.author             = { "wysaid" => "this@wysaid.org" }
  s.platform     = :ios, "13.0"
  s.osx.deployment_target = "10.13"
  s.source       = { :git => "https://github.com/wysaid/CameraCapture", :tag => "#{s.version}" }

  s.pod_target_xcconfig = {
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++17',
    'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) GLES_SILENCE_DEPRECATION=1',
  }

  s.user_target_xcconfig = {
    'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) GLES_SILENCE_DEPRECATION=1',
  }

  s.source_files = 'src/**/*.{h,hpp,c,cpp,mm,m}', 'include/**/*.{h,hpp,c,cpp,mm,m}'

  s.ios.frameworks = 'Foundation', 'AVFoundation', 'CoreVideo', 'CoreMedia', 'Accelerate'
  s.osx.frameworks = 'Foundation', 'AVFoundation', 'CoreVideo', 'CoreMedia', 'Accelerate'

end
