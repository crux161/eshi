Pod::Spec.new do |s|
  s.name             = 'larimar'
  s.version          = '0.1.0-dev.2'
  s.summary          = 'Larimar macOS external-texture host.'
  s.description      = <<-DESC
IOSurface-backed Flutter textures for the Larimar game engine.
                       DESC
  s.homepage         = 'https://github.com/crux161/eshi'
  s.license          = { :type => 'MIT' }
  s.author           = 'Larimar contributors'
  s.source           = { :path => '.' }
  s.source_files     = 'larimar/Sources/larimar/**/*.{h,mm}'
  s.public_header_files = 'larimar/Sources/larimar/include/**/*.h'
  s.dependency 'FlutterMacOS'
  s.platform         = :osx, '10.15'
  # Objective-C++ does not emit Swift-style framework autolink metadata, so
  # explicitly link the Flutter engine framework supplied by Flutter tooling.
  s.frameworks       = 'FlutterMacOS', 'CoreVideo', 'Metal', 'IOSurface'
  s.pod_target_xcconfig = {
    'DEFINES_MODULE' => 'YES',
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++11',
  }
end
