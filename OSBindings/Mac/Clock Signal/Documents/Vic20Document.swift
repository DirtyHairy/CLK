//
//  Vic20Document.swift
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Foundation

class Vic20Document: MachineDocument {

	private lazy var vic20 = CSVic20()
	override var machine: CSMachine! {
		get {
			return vic20
		}
	}
	override var name: String! {
		get {
			return "vic20"
		}
	}

	// MARK: NSDocument overrides
	override init() {
		super.init()

		if let kernel = rom("kernel-ntsc"), basic = rom("basic"), characters = rom("characters-english") {
			vic20.setKernelROM(kernel)
			vic20.setBASICROM(basic)
			vic20.setCharactersROM(characters)
		}

		if let drive = dataForResource("1540", ofType: "bin", inDirectory: "ROMImages/Commodore1540") {
			vic20.setDriveROM(drive)
		}
	}

	override class func autosavesInPlace() -> Bool {
		return true
	}

	override var windowNibName: String? {
		return "Vic20Document"
	}

	override func readFromURL(url: NSURL, ofType typeName: String) throws {
		if let pathExtension = url.pathExtension {
			switch pathExtension.lowercaseString {
				case "tap":	vic20.openTAPAtURL(url)
				case "g64":	vic20.openG64AtURL(url)
				case "d64":	vic20.openD64AtURL(url)
				default:
					let fileWrapper = try NSFileWrapper(URL: url, options: NSFileWrapperReadingOptions(rawValue: 0))
					try self.readFromFileWrapper(fileWrapper, ofType: typeName)
			}
		}
	}

	// MARK: machine setup
	private func rom(name: String) -> NSData? {
		return dataForResource(name, ofType: "bin", inDirectory: "ROMImages/Vic20")
	}

	override func readFromData(data: NSData, ofType typeName: String) throws {
		vic20.setPRG(data)
	}

	@IBOutlet var loadAutomaticallyButton: NSButton!
	var autoloadingUserDefaultsKey: String {
		get { return prefixedUserDefaultsKey("autoload") }
	}
	@IBAction func setShouldLoadAutomatically(sender: NSButton!) {
		let loadAutomatically = sender.state == NSOnState
		vic20.shouldLoadAutomatically = loadAutomatically
		self.loadAutomaticallyButton.state = loadAutomatically ? NSOnState : NSOffState
	}
	override func establishStoredOptions() {
		super.establishStoredOptions()

		let standardUserDefaults = NSUserDefaults.standardUserDefaults()
		standardUserDefaults.registerDefaults([
			autoloadingUserDefaultsKey: true
		])

		let loadAutomatically = standardUserDefaults.boolForKey(self.autoloadingUserDefaultsKey)
		vic20.shouldLoadAutomatically = loadAutomatically
		self.loadAutomaticallyButton.state = loadAutomatically ? NSOnState : NSOffState
	}
}
